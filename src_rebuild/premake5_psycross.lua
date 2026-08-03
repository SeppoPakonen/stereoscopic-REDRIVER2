-- if you want to build project with PsyCross - you have to include it to your workspace

-- Psy-Cross layer
project "PsyCross"
    kind "StaticLib"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    defines { GAME_REGION }

    files {
        "PsyCross/**.h", 
        "PsyCross/**.H", 
        "PsyCross/**.c", 
        "PsyCross/**.C", 
        "PsyCross/**.cpp",
        "PsyCross/**.CPP",
    }

    includedirs { 
        SDL2_DIR.."/include",
        OPENAL_DIR.."/include",
		"PsyCross/include"
    }

    filter {"system:Windows", "toolset:gcc"}
        includedirs {
            (os.getenv("MINGW32_INCLUDE") or "/usr/local/include").."/SDL2",
            os.getenv("MINGW32_INCLUDE") or "/usr/local/include",
        }

    filter {"system:Windows", "not toolset:gcc"}
	    defines { "_WINDOWS" }
        links { 
            "opengl32",
            "SDL2", 
            "OpenAL32"
        }

	filter {"system:Windows", "toolset:gcc"}
	    links {
            "opengl32",
            "SDL2",
            "openal"
        }
        libdirs {
            os.getenv("MINGW32_LIB") or "/usr/local/lib",
        }

	filter {"system:Windows", "platforms:x86"}
		libdirs { 
			SDL2_DIR.."/lib/x86",
			OPENAL_DIR.."/libs/Win32",
		}

	filter {"system:Windows", "platforms:x64"}
		libdirs { 
			SDL2_DIR.."/lib/x64",
			OPENAL_DIR.."/libs/Win64",
		}

    filter "system:linux"
        includedirs {
            "/usr/include/SDL2"
        }

        links {
            "GL",
            "openal",
            "SDL2",
        }

    filter "configurations:Release"
        optimize "Speed"

	filter "configurations:Release_dev"
        optimize "Speed"

    --filter { "files:**.c", "files:**.C" }
    --    compileas "C++"

usage "PsyCross"
	links "PsyCross"
	includedirs {
		"PsyCross/include",
		"PsyCross/include/psx"
	}

	filter {"system:Windows", "not toolset:gcc"}
		links {
			"opengl32",
			"SDL2",
			"OpenAL32"
		}

	filter {"system:Windows", "toolset:gcc"}
		links {
			"opengl32",
			"SDL2",
			"openal"
		}
		libdirs {
			os.getenv("MINGW32_LIB") or "/usr/local/lib",
		}