#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/connection.hpp>

#include <mumble/MumblePlugin_v_1_0_x.h>
#include <json/json.hpp>

#include <functional>
#include <thread>
#include <set>
#ifdef OS_UNIX
	#include <unistd.h>
#else
	#include <windows.h>
#endif

using Server = websocketpp::server<websocketpp::config::asio>;
using MessagePtr = Server::message_ptr;
using Json = nlohmann::json;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

union Positions
{
	float values[18] = {};

	struct
	{
		float avPos[3], avFront[3], avTop[3], cmPos[3], cmFront[3], cmTop[3];
	};
} positions;

uint64_t gameId;
std::string userContext;
std::string userIdent;

struct MumbleAPI_v_1_0_x mumbleApi;
mumble_plugin_id_t ownId;

Server posServer;
std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> connections;
bool initializedServerBefore = false;
std::thread serverThread;

#ifdef OS_UNIX
	uint64_t robloxPid;
#else
	HANDLE robloxProcHandle;
#endif

// Define a callback to handle incoming messages
void onMessage(Server* s, websocketpp::connection_hdl hdl, MessagePtr msg)
{
	// Store connection
	connections.emplace(hdl);

	// Parse received JSON, write values to positions array
	Json decoded = Json::parse(msg->get_payload());
	for (int i = 0; i < 18; ++i)
		positions.values[i] = decoded[i / 3][i % 3];

	// ROBLOX Studs -> Meters conversion
	for (int i = 0; i < 3; ++i)
	{
		positions.avPos[i] *= 0.28f;
		positions.cmPos[i] *= 0.28f;
	}

	gameId = decoded[6];
	userIdent = decoded[7];
}

void threadLoop()
{
	try
	{
		// If we've started this thread before, prepare the asio io_service loop to run again
		if (initializedServerBefore)
		{
			posServer.get_io_service().restart();
		}
		else
		{
			initializedServerBefore = true;
		}

		// Listen on port 9002
		posServer.listen(9002);

		// Start the server accept loop
		posServer.start_accept();

		// Start the ASIO io_service run loop
		posServer.run();
	}
	catch (const websocketpp::exception& e)
	{
		std::string logMsg = "Websocket thread error (MAKE A GITHUB ISSUE): ";
		mumbleApi.log(ownId, (logMsg + e.m_msg).c_str());
	}
}

bool gameRunning()
{
#ifdef OS_UNIX
	if (getpgid(robloxPid) >= 0) {
		return false;
	}
#else
	DWORD code = 0;
	if (GetExitCodeProcess(robloxProcHandle, &code) == 0)
	{
		CloseHandle(robloxProcHandle);
		return false;
	}

	if (code != STILL_ACTIVE)
	{
		CloseHandle(robloxProcHandle);
		return false;
	}
#endif
	return true;
}

// Called by Mumble to fetch positional and rotational character data
bool mumble_fetchPositionalData(float* avatarPos, float* avatarDir, float* avatarAxis, float* cameraPos,
                                float* cameraDir,
                                float* cameraAxis, const char** context, const char** identity)
{
	// Set data
	memcpy(avatarPos, positions.avPos, sizeof(positions.avPos));
	memcpy(avatarDir, positions.avFront, sizeof(positions.avFront));
	memcpy(avatarAxis, positions.avTop, sizeof(positions.avTop));
	memcpy(cameraPos, positions.cmPos, sizeof(positions.cmPos));
	memcpy(cameraDir, positions.cmFront, sizeof(positions.cmFront));
	memcpy(cameraAxis, positions.cmTop, sizeof(positions.cmTop));

	// Set context so it's only for people in the same game
	userContext = std::to_string(gameId);
	*context = userContext.c_str();

	// Set identity.
	*identity = userIdent.c_str();

	// Stop fetching if game is closed
	return gameRunning();
}

uint8_t mumble_initPositionalData(const char* const * programNames, const uint64_t* programPIDs, const size_t programCount)
{
	// Check if game is open
	for (int i = 0; i < programCount; ++i)
	{
		if (strcmp(programNames[i], "RobloxPlayerBeta.exe") == 0)
		{
			// Initialize handle and check whether or not it's valid
#ifdef OS_UNIX
				robloxPid = programPIDs[i];
#else
			robloxProcHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(programPIDs[i]));
			if (robloxProcHandle == nullptr)
			{
				return MUMBLE_PDEC_ERROR_TEMP;
			}
#			endif

			// Check if ROBLOX is still open
			if (!gameRunning())
			{
#ifndef OS_UNIX
				CloseHandle(robloxProcHandle);
#endif
				return MUMBLE_PDEC_ERROR_TEMP;
			}

			serverThread = std::thread(threadLoop);

			return MUMBLE_PDEC_OK;
		}
	}

	return MUMBLE_PDEC_ERROR_TEMP;
}

// Stop the websocket listening and join the thread
void mumble_shutdownPositionalData()
{
	// Cleanly stop the websocket server
	posServer.stop_listening();

	// Close all connections
	for (auto& connectionHdl : connections)
	{
		auto connection = posServer.get_con_from_hdl(connectionHdl);

		// Make sure connection is open before closing it
		if (connection->get_state() == websocketpp::session::state::open)
		{
			connection->close(websocketpp::close::status::normal, "Server shutdown");
		}
	}
	connections.clear();

	// Join server thread to main thread
	if (serverThread.joinable())
	{
		serverThread.join();
	}
}

// Plugin initialization routine
mumble_error_t mumble_init(mumble_plugin_id_t pluginID)
{
	// Set plugin ID
	ownId = pluginID;

	// Initialize websocket server
	try
	{
		// Set logging settings
		posServer.set_access_channels(websocketpp::log::alevel::all);
		posServer.clear_access_channels(websocketpp::log::alevel::frame_payload);

		// Initialize ASIO
		posServer.init_asio();

		// Register our message handler
		posServer.set_message_handler(bind(&onMessage, &posServer, _1, _2));
	}
	catch (const websocketpp::exception& e)
	{
		std::string logMsg = "Plugin init error (MAKE A GITHUB ISSUE): ";
		mumbleApi.log(ownId, (logMsg + e.m_msg).c_str());
	}

	return MUMBLE_STATUS_OK;
}

// We don't need to do anything on shutdown
void mumble_shutdown()
{
}

// Mumble boilerplate
mumble_version_t mumble_getVersion()
{
	mumble_version_t version;
	version.major = 2;
	version.minor = 0;
	version.patch = 1;

	return version;
}

struct MumbleStringWrapper mumble_getAuthor()
{
	static auto author = "checkraisefold, bonezone2001";

	struct MumbleStringWrapper wrapper;
	wrapper.data = author;
	wrapper.size = strlen(author);
	wrapper.needsReleasing = false;

	return wrapper;
}

struct MumbleStringWrapper mumble_getDescription()
{
	static auto description =
		"A plugin using a websocket server to support ROBLOX. Context and identity are supported.";

	struct MumbleStringWrapper wrapper;
	wrapper.data = description;
	wrapper.size = strlen(description);
	wrapper.needsReleasing = false;

	return wrapper;
}

struct MumbleStringWrapper mumble_getName()
{
	static auto name = "ROBLOXLink";

	struct MumbleStringWrapper wrapper;
	wrapper.data = name;
	wrapper.size = strlen(name);
	wrapper.needsReleasing = false;

	return wrapper;
}


mumble_version_t mumble_getAPIVersion()
{
	// This constant will always hold the API version that fits the included header files
	return MUMBLE_PLUGIN_API_VERSION;
}

void mumble_registerAPIFunctions(void* apiStruct)
{
	// Provided mumble_getAPIVersion returns MUMBLE_PLUGIN_API_VERSION, this cast will make sure
	// that the passed pointer will be cast to the proper type
	mumbleApi = MUMBLE_API_CAST(apiStruct);
}

void mumble_releaseResource(const void* pointer)
{
}

uint32_t mumble_getFeatures()
{
	return MUMBLE_FEATURE_POSITIONAL;
}
