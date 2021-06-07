#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <mumble/MumblePlugin_v_1_0_x.h>

#include <json/json.hpp>

#include <functional>
#include <iostream>
#include <thread>
#include <mutex>

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

INT64 gameID = 0;
std::string userIdent;

struct MumbleAPI_v_1_0_x mumbleAPI;
mumble_plugin_id_t ownID;

Server posServer;

// Called by Mumble to fetch positional and rotational character data
bool mumble_fetchPositionalData(float* avatarPos, float* avatarDir, float* avatarAxis, float* cameraPos, float* cameraDir,
	float* cameraAxis, const char** context, const char** identity) {	
	// Set data
	memcpy(avatarPos, pos.avPos, sizeof(pos.avPos));
	memcpy(avatarDir, pos.avFront, sizeof(pos.avFront));
	memcpy(avatarAxis, pos.avTop, sizeof(pos.avTop));
	memcpy(cameraPos, pos.cmPos, sizeof(pos.cmPos));
	memcpy(cameraDir, pos.cmFront, sizeof(pos.cmFront));
	memcpy(cameraAxis, pos.cmTop, sizeof(pos.cmTop));

	// Set context so it's only for people in the same game
	*context = std::to_string(gameID).c_str();

	// Set identity.
	*identity = userIdent.c_str();

	return true;
}

// Define a callback to handle incoming messages
void onMessage(Server* s, websocketpp::connection_hdl hdl, MessagePtr msg)
{
	// Parse received json, write values to positions array
	json decoded = json::parse(msg->get_payload());
	for (int i = 0; i < 18; i++)
		pos.values[i] = decoded[i / 3][i % 3];

	// ROBLOX Studs -> Meters conversion
	pos.avPos[0] *= 0.28f;
	pos.avPos[1] *= 0.28f;
	pos.avPos[2] *= 0.28f;

	pos.cmPos[0] *= 0.28f;
	pos.cmPos[1] *= 0.28f;
	pos.cmPos[2] *= 0.28f;

	gameID = decoded[6];
	userIdent = decoded[7];
}

void ThreadLoop() {
	try {
		// Set logging settings
		posServer.set_access_channels(websocketpp::log::alevel::all);
		posServer.clear_access_channels(websocketpp::log::alevel::frame_payload);

		// Initialize ASIO
		posServer.init_asio();

		// Register our message handler
		posServer.set_message_handler(bind(&onMessage, &posServer, ::_1, ::_2));

		// Listen on port 9002
		posServer.listen(9002);

		// Start the server accept loop
		posServer.start_accept();

		// Start the ASIO io_service run loop
		posServer.run();
	}
	catch (websocketpp::exception const& e) {
	}
	catch (...) {
	}
}

uint8_t mumble_initPositionalData(const char* const *programNames, const uint64_t programPIDs, size_t programCount)
{
	return PDEC_OK;
}

// Empty for now
void mumble_shutdownPositionalData() { }

// Initialize websocket thread on plugin init
mumble_error_t mumble_init(mumble_plugin_id_t pluginID) {
	ownID = pluginID;
	serverThread = std::thread(ThreadLoop);

	return STATUS_OK;
}

// Join the server thread to the main thread for a clean shutdown
void mumble_shutdown() {
	posServer.stop_listening();
	if (serverThread.joinable()) {
		serverThread.join();
	}
}


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
	static const char* description = "A plugin using a websocket server to support ROBLOX.";

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
	// This constant will always hold the API version  that fits the included header files
	return MUMBLE_PLUGIN_API_VERSION;
}

void mumble_registerAPIFunctions(void* apiStruct) {
	// Provided mumble_getAPIVersion returns MUMBLE_PLUGIN_API_VERSION, this cast will make sure
	// that the passed pointer will be cast to the proper type
	mumbleAPI = MUMBLE_API_CAST(apiStruct);
}

void mumble_releaseResource(const void* pointer) {
	// As we never pass a resource to Mumble that needs releasing, this function should never
	// get called
	printf("Called mumble_releaseResource but expected that this never gets called -> Aborting");
	abort();
}

uint32_t mumble_getFeatures() {
	return FEATURE_POSITIONAL;
}