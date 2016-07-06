solution "gmsv_enginelog"

	language "C++"
	location ( os.get() .."-".. _ACTION )
	flags { "Symbols", "NoEditAndContinue", "NoPCH", "StaticRuntime", "EnableSSE", "EnableSSE2" }
	targetdir ( "lib/" .. os.get() .. "/" )
	includedirs { "include/" }

	platforms { "x32"}
	
	configurations
	{ 
		"Release"
	}
	
	configuration "Release"
		defines { "NDEBUG" }
		flags{ "OptimizeSpeed", "FloatStrict", "ExtraWarnings" }
	
	project "gmsv_enginelog"
		defines { "GMMODULE" } 
--		links { "rt", "crypto" }
		files { "src/**.*", "../include/**.*" }
		kind "SharedLib"
		
