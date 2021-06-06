#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <mumble/mumble_legacy_plugin.h>
#include <mumble/mumble_positional_audio_main.h>

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
INT64 gameID;
bool initialized = false;
bool listening = false;

static std::wstring description(L"RobloxLINK (v1.0)");
static std::wstring shortname(L"RobloxLINK");

static const std::wstring longdesc() {
	return std::wstring(L"Supports Roblox with context and identity."); // Plugin long description
}


// That good good
static int fetch(float* avatarPos, float* avatarFront, float* avatarTop, float* cameraPos, float* cameraFront, float* cameraTop, std::string& context, std::wstring& identity)
{	
	memcpy(avatarPos, pos.avPos, sizeof(pos.avPos));
	memcpy(avatarFront, pos.avFront, sizeof(pos.avFront));
	memcpy(avatarTop, pos.avTop, sizeof(pos.avTop));
	memcpy(cameraPos, pos.cmPos, sizeof(pos.cmPos));
	memcpy(cameraFront, pos.cmFront, sizeof(pos.cmFront));
	memcpy(cameraTop, pos.cmTop, sizeof(pos.cmTop));

	// Set context so it's only for people in the same game
	context = std::to_string(gameID);

	// All use the same VC, just set this to satisfy mumble if it's cringe
	identity = L"Shared";

	return true;
}

// Define a callback to handle incoming messages
void onMessage(Server* s, websocketpp::connection_hdl hdl, MessagePtr msg)
{
	// Check for command to stop listening
	if (msg->get_payload() == "stop-listening")
	{
		listening = false;
		s->stop_listening();
		return;
	}

	// Get positions and do meters conversion from studs
	json decoded = json::parse(msg->get_payload());
	for (int i = 0; i < 18; i++)
		pos.values[i] = decoded[i / 3][i % 3];

	pos.avPos[0] *= 0.28f;
	pos.avPos[1] *= 0.28f;
	pos.avPos[2] *= 0.28f;

	pos.cmPos[0] *= 0.28f;
	pos.cmPos[1] *= 0.28f;
	pos.cmPos[2] *= 0.28f;

	gameID = decoded[6];
}

void ServerThread() {
	Server posServer;
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

static int trylock(const std::multimap<std::wstring, unsigned long long int>& pids)
{
	// Initialize the values to one (cos mumble gay)
	for (int i = 0; i < 18; i++) pos.values[i] = 1.f;
	gameID = 0;

	// Retrieve game executable's memory address
	if (!initialize(pids, L"RobloxPlayerBeta.exe"))
		return false;

	if (!initialized)
	{
		initialized = true;
		serverThread = std::thread(ServerThread);

		AllocConsole();
		freopen("conin$", "r", stdin);
		freopen("conout$", "w", stdout);
		freopen("conout$", "w", stderr);
	}

	// Check if we can get meaningful data from it
	float apos[3], afront[3], atop[3], cpos[3], cfront[3], ctop[3];
	std::wstring identity;
	std::string context;

	if (fetch(apos, afront, atop, cpos, cfront, ctop, context, identity))
		return true;

	generic_unlock();
	return false;
}

static int trylock1()
{
	return trylock(std::multimap<std::wstring, unsigned long long int>());
}

static MumblePlugin gameplug = {
	MUMBLE_PLUGIN_MAGIC,
	description,
	shortname,
	NULL,
	NULL,
	trylock1,
	generic_unlock,
	longdesc,
	fetch
};

static MumblePlugin2 gameplug2 = {
	MUMBLE_PLUGIN_MAGIC_2,
	MUMBLE_PLUGIN_VERSION,
	trylock
};

extern "C" MUMBLE_PLUGIN_EXPORT MumblePlugin * getMumblePlugin() {
	return &gameplug;
}

extern "C" MUMBLE_PLUGIN_EXPORT MumblePlugin2 * getMumblePlugin2() {
	return &gameplug2;
}
