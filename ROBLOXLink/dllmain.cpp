#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/connection.hpp>

#include <mumble/MumblePlugin_v_1_0_x.h>

#include <json/json.hpp>

#include <functional>
#include <iostream>
#include <thread>
#include <mutex>
#include <set>

using Server = websocketpp::server<websocketpp::config::asio>;
using MessagePtr = Server::message_ptr;
using json = nlohmann::json;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

std::thread serverThread;

union positions
{
	float values[18] = { };
	struct {
		float avPos[3], avFront[3], avTop[3], cmPos[3], cmFront[3], cmTop[3];
	};
};
positions pos;

INT64 gameID;
std::string userContext;
std::string userIdent;

struct MumbleAPI_v_1_0_x mumbleAPI;
mumble_plugin_id_t ownID;

Server posServer;
std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> connections;
bool initializedServerBefore = false;

HANDLE robloxProcHandle;

// Called by Mumble to fetch positional and rotational character data
bool mumble_fetchPositionalData(float* avatarPos, float* avatarDir, float* avatarAxis, float* cameraPos, float* cameraDir,
	float* cameraAxis, const char** context, const char** identity) {	
	// Check if ROBLOX is still open
	DWORD code = 0;
	if (GetExitCodeProcess(robloxProcHandle, &code) == 0) {
		CloseHandle(robloxProcHandle);
		return false;
	}

	if (code != STILL_ACTIVE) {
		CloseHandle(robloxProcHandle);
		return false;
	}

	// Set data
	memcpy(avatarPos, pos.avPos, sizeof(pos.avPos));
	memcpy(avatarDir, pos.avFront, sizeof(pos.avFront));
	memcpy(avatarAxis, pos.avTop, sizeof(pos.avTop));
	memcpy(cameraPos, pos.cmPos, sizeof(pos.cmPos));
	memcpy(cameraDir, pos.cmFront, sizeof(pos.cmFront));
	memcpy(cameraAxis, pos.cmTop, sizeof(pos.cmTop));

	// Set context so it's only for people in the same game
	userContext = std::to_string(gameID);
	*context = userContext.c_str();

	// Set identity.
	*identity = userIdent.c_str();

	return true;
}

// Define a callback to handle incoming messages
void onMessage(Server* s, websocketpp::connection_hdl hdl, MessagePtr msg)
{
	// Store connection
	connections.emplace(hdl);

	// Parse received json, write values to positions array
	json decoded = json::parse(msg->get_payload());
	for (int i = 0; i < 18; i++)
		pos.values[i] = decoded[i / 3][i % 3];

	// ROBLOX Studs -> Meters conversion
	for (int i = 0; i < 3; i++) {
		pos.avPos[i] *= 0.28f;
		pos.cmPos[i] *= 0.28f;
	}

	gameID = decoded[6];
	userIdent = decoded[7];

	s->send(hdl, std::to_string(pos.avPos[1]), msg->get_opcode());
}

void threadLoop() {
	try {
		if (!initializedServerBefore) {
			// Flip variable
			initializedServerBefore = true;

			// Listen on port 9002
			posServer.listen(9002);

			// Start the server accept loop
			posServer.start_accept();

			// Start the ASIO io_service run loop
			posServer.run();
		}
		else {
			// Prepare io_service to run again
			posServer.get_io_service().restart();

			// Listen on port 9002
			posServer.listen(9002);

			// Start the ASIO io_service run loop
			posServer.run();
		}
	}
	catch (websocketpp::exception const& e) {
		std::string logMsg = "Websocket thread error (MAKE A GITHUB ISSUE): ";
		mumbleAPI.log(ownID, (logMsg + e.m_msg).c_str());
	}
}

uint8_t mumble_initPositionalData(const char* const *programNames, const uint64_t *programPIDs, size_t programCount)
{
	// Check if game is open
	for (int i = 0; i < programCount; i++) {
		if (strcmp(programNames[i], "RobloxPlayerBeta.exe") == 0) {
			// Initialize handle and check whether or not it's valid
			robloxProcHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(programPIDs[i]));
			if (robloxProcHandle == NULL) {
				return MUMBLE_PDEC_ERROR_TEMP;
			}

			// Check if ROBLOX is still open
			DWORD code = 0;
			if (GetExitCodeProcess(robloxProcHandle, &code) == 0) {
				CloseHandle(robloxProcHandle);
				return MUMBLE_PDEC_ERROR_TEMP;
			}

			if (code != STILL_ACTIVE) {
				CloseHandle(robloxProcHandle);
				return MUMBLE_PDEC_ERROR_TEMP;
			}

			serverThread = std::thread(threadLoop);

			return MUMBLE_PDEC_OK;
		}
	}

	return MUMBLE_PDEC_ERROR_TEMP;
}

// Stop the websocket listening and join the thread
void mumble_shutdownPositionalData() { 
	// Cleanly stop the websocket server
	posServer.stop_listening();
	for (auto& connection : connections) {
		try {
			posServer.get_con_from_hdl(connection)->close(1000, "Server shutdown");
		}
		catch (websocketpp::exception const& e) { }
	}
	connections.clear();

	if (serverThread.joinable()) {
		serverThread.join();
	}
}

// Plugin initialization routine
mumble_error_t mumble_init(mumble_plugin_id_t pluginID) {
	// Set plugin ID
	ownID = pluginID;

	// Initialize websocket server
	try {
		// Set logging settings
		posServer.set_access_channels(websocketpp::log::alevel::all);
		posServer.clear_access_channels(websocketpp::log::alevel::frame_payload);

		// Initialize ASIO
		posServer.init_asio();

		// Register our message handler
		posServer.set_message_handler(bind(&onMessage, &posServer, ::_1, ::_2));
	}
	catch (websocketpp::exception const& e) {
		std::string logMsg = "Plugin init error (MAKE A GITHUB ISSUE): ";
		mumbleAPI.log(ownID, (logMsg + e.m_msg).c_str());
	}

	return MUMBLE_STATUS_OK;
}

// We don't need to do anything on shutdown
void mumble_shutdown() { }


// Mumble boilerplate
mumble_version_t mumble_getVersion() {
	mumble_version_t version;
	version.major = 2;
	version.minor = 0;
	version.patch = 0;

	return version;
}

struct MumbleStringWrapper mumble_getAuthor() {
	static const char* author = "checkraisefold, bonezone2001";

	struct MumbleStringWrapper wrapper;
	wrapper.data = author;
	wrapper.size = strlen(author);
	wrapper.needsReleasing = false;

	return wrapper;
}

struct MumbleStringWrapper mumble_getDescription() {
	static const char* description = "A plugin using a websocket server to support ROBLOX. Context and identity are supported.";

	struct MumbleStringWrapper wrapper;
	wrapper.data = description;
	wrapper.size = strlen(description);
	wrapper.needsReleasing = false;

	return wrapper;
}

struct MumbleStringWrapper mumble_getName() {
	static const char* name = "ROBLOXLink";

	struct MumbleStringWrapper wrapper;
	wrapper.data = name;
	wrapper.size = strlen(name);
	wrapper.needsReleasing = false;

	return wrapper;
}


mumble_version_t mumble_getAPIVersion() {
	// This constant will always hold the API version that fits the included header files
	return MUMBLE_PLUGIN_API_VERSION;
}

void mumble_registerAPIFunctions(void* apiStruct) { 
	// Provided mumble_getAPIVersion returns MUMBLE_PLUGIN_API_VERSION, this cast will make sure
	// that the passed pointer will be cast to the proper type
	mumbleAPI = MUMBLE_API_CAST(apiStruct); 
}

void mumble_releaseResource(const void* pointer) { }

uint32_t mumble_getFeatures() {
	return MUMBLE_FEATURE_POSITIONAL;
}