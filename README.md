# ROBLOXLink
A [Mumble](https://github.com/mumble-voip/mumble) plugin that receives positional and rotational data from a websocket. Designed for a ROBLOX Lua script executed with a scripting utility.

The client Lua script can be found [here.](https://github.com/checkraisefold/ROBLOXLink-Client)

# Contributors
[@bonezone2001](https://github.com/bonezone2001) helped out with a large majority of the coding. If it wasn't for him, this project wouldn't be even close to a completed state.

[@Krzmbrzl](https://github.com/Krzmbrzl) and [@davidebeatrici](https://github.com/davidebeatrici) are Mumble contributors that were both a massive help in the plugin creation process. Specifically, Krzmbrzl created the new plugin framework used in this plugin and wrote the documentation for it, and davidebeatrici was very helpful in answering my C++ noob questions.

# Instructions
1. Download the latest release of the ROBLOXLink mumble plugin from [here.](https://github.com/checkraisefold/ROBLOXLink-Plugin/releases)
2. Download Mumble 1.4.0 RC1 from [here.](https://dl.mumble.info/latest/snapshot/client-windows-x64)
3. Open it and complete the setup. Go to Configure -> Settings -> Audio Output and set Minimum Volume to 0%
4. Change to the Plugins tab, press "Install plugin" and select the file path to the downloaded .mumble_plugin from the Github, or alternatively close Mumble and open the 1. mumble_plugin and it will open with Mumble and install.
5. **IMPORTANT:** Go to the Plugins tab if you're not in it already, turn on "Link to game and transmit position" at the top or else the plugin WILL NOT WORK!
6. Join a server with other users. I recommend using [GuildBit](https://guildbit.com/) to create a free temporary Mumble server.
7. Join the same game and server with the other users. 
8. Execute the script below with a scripting utility. Chat away!

```
loadstring(game:HttpGet("https://raw.githubusercontent.com/checkraisefold/ROBLOXLink-Client/main/client.lua"))()
```

This script supports: Synapse X, Scriptware (important - plugin also has macos support!), Protosmasher, and Krnl.
