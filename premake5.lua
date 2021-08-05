workspace "ROBLOXLink"
    configurations { "Debug", "Release" }
	
	filter "action:vs*"
		buildoptions { "/Zc:__cplusplus" }
		platforms { "Win64" }
		
	filter "action:gmake*"
		platforms { "Linux64", "Mac64" }
	
	filter "Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "Release"
        defines { "NDEBUG" }
        optimize "On"
		
	filter "platforms:*64"
		architecture "x86_64"
		
	filter "platforms:Win64" 
		system "windows"
		
	filter "platforms:Linux64"
		system "linux"
		defines { "OS_UNIX" }
		
	filter "platforms:Mac64"
		system "macosx"
		defines { "OS_UNIX" }

project "ROBLOXLink"
    kind "SharedLib"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
	includedirs { "libraries" }
	defines { "_CRT_SECURE_NO_WARNINGS", "_SCL_SECURE_NO_WARNINGS", "ASIO_STANDALONE" }

    files { "src/**.hpp", "src/**.h", "src/**.cpp" }