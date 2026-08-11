-- premake5.lua

require "premake_modules/usage"
require "premake_modules/emscripten"
require "premake_modules/vscode"

IS_ANDROID = (_ACTION == "androidndk")

------------------------------------------

newoption {
   trigger     = "raspberry-pi",
   description = "adds specific define for compiling on Raspberry Pi"
}

table.insert(premake.option.get("os").allowed, { "emscripten", "Emscripten" })

------------------------------------------

-- you can redefine dependencies
SDL2_DIR = os.getenv("SDL2_DIR") or "dependencies/SDL2"
OPENAL_DIR = os.getenv("OPENAL_DIR") or "dependencies/openal-soft"
JPEG_DIR = os.getenv("JPEG_DIR") or "dependencies/jpeg"

WEBDEMO_DIR = os.getenv("WEBDEMO_DIR") or "../../web_demo@/"	-- FIXME: make it better
RED2_DIR = os.getenv("RED2_DIR") or "../../data@/"
WEBSHELL_PATH = "../platform/Emscripten"	-- must be relative to makefile path (SADLY)

GAME_REGION = os.getenv("GAME_REGION") or "NTSC_VERSION" -- or PAL_VERSION
GAME_VERSION = os.getenv("APPVEYOR_BUILD_VERSION") or nil

if not (GAME_REGION == "NTSC_VERSION" or GAME_REGION == "PAL_VERSION") then
    error("'GAME_REGION' should be 'NTSC_VERSION' or 'PAL_VERSION'")
end

------------------------------------------
	
workspace "REDRIVER2"
	if _ACTION ~= "vscode" then
		if os.target() == "emscripten" then
    		location "build_web"
		else
    		location "build"
		end
	else
		-- setup VSCode generator settings
		vscode_makefile "build"
		vscode_launch_cwd ("${workspaceRoot}/../data")
		--vscode_launch_environment {	}
	end

    configurations { "Debug", "Release", "Release_dev" }
	
    defines { VERSION } 
	
	if os.target() == "emscripten" then
		platforms { "emscripten" }
	
		buildoptions {
			"-s USE_SDL=2",
			"-s USE_LIBJPEG=1",
			--"-s USE_WEBGL2=1",
			"-Wno-c++11-narrowing",
			"-Wno-constant-conversion",
			"-Wno-writable-strings",
			"-Wno-unused-value",
			"-Wno-switch",
			"-Wno-shift-op-parentheses",
			"-Wno-parentheses",
			"-Wno-format",
		}

		linkoptions  { 
			"-s TOTAL_MEMORY=1073741824",
			"-s USE_SDL=2",
			"-s USE_LIBJPEG=1",
			"-s FULL_ES2=1",
			--"-s USE_WEBGL2=1",
			"-s ASYNCIFY=1",
			"-s ALLOW_MEMORY_GROWTH=1",
			"-s GL_TESTING=1",
			("--shell-file " .. WEBSHELL_PATH .. "/shell.html"),
			("--preload-file " .. WEBDEMO_DIR),
			("--preload-file " .. RED2_DIR),
			"-s 'EXPORTED_RUNTIME_METHODS=[\"ccall\", \"writeArrayToMemory\"]'",
			"-s 'EXPORTED_FUNCTIONS=[\"_main\", \"_malloc\"]'"
		}
		
		filter { "kind:*App" }
			targetextension ".html"
			
		postbuildcommands {
			"{COPY} " .. WEBSHELL_PATH .. "/style.css %{cfg.buildtarget.directory}",
			"{COPY} " .. WEBSHELL_PATH .. "/lsfs.js %{cfg.buildtarget.directory}"
		}

	elseif IS_ANDROID then		
		system "android"
		shortcommands "On"
		
		platforms {
			"android-arm", "android-arm64"
		}
		
		disablewarnings {
			"c++11-narrowing",
			"constant-conversion",
			"writable-strings",
			"unused-value",
			"switch",
			"shift-op-parentheses",
			"parentheses",
			"format",
		}
		
		buildoptions {
			"-fpermissive",
			"-fexceptions",
			"-pthread",
		}
		
		linkoptions {
			"--no-undefined",
			"-fexceptions",
			"-pthread",
			
			"-mfloat-abi=softfp",	-- force NEON to be used
			"-mfpu=neon"
		}

		filter "platforms:*-x86"
			architecture "x86"

		filter "platforms:*-x64"
			architecture "x64"

		filter "platforms:*-arm"
			architecture "arm"

		filter "platforms:*-arm64"
			architecture "arm64"
	else
		platforms { "x86", "x64" }
	end
	
	startproject "REDRIVER2"
	
	configuration "raspberry-pi"
		defines { "__RPI__" }

	filter "system:Linux"
		buildoptions {
            "-Wno-narrowing",
			"-Wno-endif-labels",
			"-Wno-write-strings",
			"-Wno-format-security",
			"-Wno-unused-result",
            "-fpermissive"
        }
		
		cppdialect "C++11"
		
	filter {"system:Linux", "platforms:x86"}
		buildoptions { "-m32" }
		linkoptions { "-m32" }

	filter "system:Windows"
		disablewarnings { "4996", "4554", "4244", "4101", "4838", "4309" }

    filter "configurations:Debug"
        defines { 
            "_DEBUG", 
	        "DEBUG"
        }
        symbols "On"

    filter "configurations:Release"
        defines {
            "NDEBUG",
        }
		
	filter "configurations:Release_dev"
        defines {
            "NDEBUG",
        }
        symbols "On"
        
if os.target() == "windows" then
	include "premake_libjpeg.lua"
end

-- font tool
if os.target() ~= "emscripten" then
	include "premake5_font_tool.lua"
end

-- Psy-Cross layer
include "premake5_psycross.lua"



-- game iteslf
project "REDRIVER2"
    kind "WindowedApp"

    language "c++"
    targetdir "bin/%{cfg.buildcfg}"

    includedirs { 
        "Game", 
    }
	
	uses { 
		"PsyCross",
	}

    defines { GAME_REGION }
	defines { "BUILD_CONFIGURATION_STRING=\"%{cfg.buildcfg}\"" }
	
	if GAME_VERSION ~= nil then
		local resVersion = string.gsub(GAME_VERSION, "%.", ",")
		defines{ "GAME_VERSION_N=\""..GAME_VERSION.."\"" }
		defines{ "GAME_VERSION_RES="..resVersion.."" }
	end

    files {
        "Game/**.h",
        "Game/**.c"
    }

    filter {"system:Windows or linux or platforms:emscripten"}
        --dependson { "PsyX" }
        links { "jpeg" }
				
		files {
			"utils/**.h",
			"utils/**.cpp",
			"utils/**.c",
			"redriver2_psxpc.cpp",
		}
		
	filter "platforms:emscripten"
	    includedirs { 
			OPENAL_DIR.."/include",
			JPEG_DIR.."/",
        }
		files { 
            "platform/Emscripten/*.cpp",
        }

    filter "system:Windows"
		entrypoint "mainCRTStartup"
		
		files { -- TEMP
			"platform/Emscripten/*.h",
			"platform/Emscripten/*.css",
			"platform/Emscripten/*.html", 
        }
		
        files { 
            "platform/Windows/resource.h", 
            "platform/Windows/Resource.rc", 
            "platform/Windows/main.ico" 
        }

        includedirs { 
            SDL2_DIR.."/include",
            OPENAL_DIR.."/include",
			JPEG_DIR.."/",
        }
    
        filter {"system:Windows", "toolset:gcc"}
            includedirs {
                (os.getenv("MINGW32_INCLUDE") or "/usr/local/include").."/SDL2",
                os.getenv("MINGW32_INCLUDE") or "/usr/local/include",
            }

        filter { "system:Windows", "toolset:msc" }
        linkoptions {
			"/SAFESEH:NO", -- Image Has Safe Exception Handers: No. Because of openal-soft
        }
        
    filter "system:linux"
        includedirs {
            "/usr/include/SDL2"
        }

        links {
            "GL",
            "openal",
            "SDL2",
            "dl",
        }

    filter "configurations:Debug"
		targetsuffix "_dbg"
        defines { 
            "DEBUG_OPTIONS",
            "COLLISION_DEBUG",
			"CUTSCENE_RECORDER"
         }
		 symbols "On"

    filter "configurations:Release"
        optimize "Speed"
		
	filter "configurations:Release_dev"
		targetsuffix "_dev"
        defines { 
            "DEBUG_OPTIONS",
            "COLLISION_DEBUG",
			"CUTSCENE_RECORDER"
        }
        optimize "Speed"

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

-- ---------------------------------------------------------------------------
-- DX11 spike (T0.5): standalone test proving the DX11 stack. Built as its own
-- executable so it does not interfere with the game binary. Links the mingw32
-- Direct3D 11 / DXGI / D3DCompiler import libraries.
-- ---------------------------------------------------------------------------
project "dx11_spike"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_spike.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 renderer foundation (T1.1): standalone harness that drives the reusable
-- dx11_renderer module (device/context/native window/swapchain/backbuffer RTV/
-- depth-stencil/offscreen RTs + present/resize/-res). Proves the full T1.1
-- stack headless via BMP capture, like the T0.5 spike.
-- ---------------------------------------------------------------------------
project "dx11_foundation"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_foundation.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 draw-command executor (T1.5): per-frame command list, view/proj + world
-- CBs, frustum culling, sorting (opaque by material F2B / translucent by depth
-- B2F), material batching and DrawIndexed emission on top of dx11_resources /
-- dx11_textures / dx11_shaders. Driven by dx11_drawcmdexec_test.cpp; verified
-- headless (cull/sort/batch/emit probes + draw-call/cull counts).
-- ---------------------------------------------------------------------------
project "dx11_drawcmdexec"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_resources.h",
        "spike/dx11_resources.c",
        "spike/dx11_textures.h",
        "spike/dx11_textures.c",
        "spike/dx11_shaders.h",
        "spike/dx11_shaders.c",
        "spike/dx11_drawcmdexec.h",
        "spike/dx11_drawcmdexec.c",
        "spike/dx11_drawcmdexec_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 MODEL -> arena adapter (T1.6): game-agnostic bridge that converts a raw
-- model mesh (int16 vertices, per-poly vertex indices + tpage-local texel UVs +
-- texture_set/texture_id + flat/gouraud color + state) into the dx11_resources
-- arena and executor commands: UV texel->normalized, texture bake via a resolve
-- hook, world + local bbox, flat/gouraud, translucent. Driven by
-- dx11_modeladapter_test.cpp on top of the full stack; verified headless
-- (texture/gouraud/batch/sort/world/cull/blend probes + counts).
-- ---------------------------------------------------------------------------
project "dx11_modeladapter"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_resources.h",
        "spike/dx11_resources.c",
        "spike/dx11_textures.h",
        "spike/dx11_textures.c",
        "spike/dx11_shaders.h",
        "spike/dx11_shaders.c",
        "spike/dx11_drawcmdexec.h",
        "spike/dx11_drawcmdexec.c",
        "spike/dx11_modeladapter.h",
        "spike/dx11_modeladapter.c",
        "spike/dx11_modeladapter_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 DirectInput8 input backend (T1.7): COM init + DirectInput8Create,
-- keyboard/mouse/joystick devices (SetDataFormat/SetCooperativeLevel/Acquire),
-- poll state (keys/mouse/joystick), and the DIK -> logical-key map mirroring
-- the game's default PSX pad controls. Driven by dx11_input_test.cpp; verified
-- headless (init/poll/map probes). Links dinput8 (DirectInput8Create) + ole32
-- (CoInitializeEx).
-- ---------------------------------------------------------------------------
project "dx11_input"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_input.h",
        "spike/dx11_input.c",
        "spike/dx11_input_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "dinput8",
            "dxguid",   -- IID_IDirectInput8A, GUID_SysKeyboard/Mouse, axis GUIDs
            "ole32",
            "user32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 XAudio2 audio backend (T1.8): XAudio2Create engine + mastering voice,
-- per-buffer source voices with a PCM format, raw 16-bit PCM submission
-- (loopable), play/stop, volume + 2-channel pan. Driven by dx11_audio_test.cpp;
-- verified headless (engine/voice/play/volpan probes). Links xaudio2_8
-- (XAudio2Create) + ole32 (CoInitializeEx).
-- ---------------------------------------------------------------------------
project "dx11_audio"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_audio.h",
        "spike/dx11_audio.c",
        "spike/dx11_audio_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "xaudio2_8",
            "ole32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 per-eye render-target verification (T2.1): renders a distinct quad per
-- eye into the two offscreen RTs (renderer + resources + textures + shaders +
-- executor) and asserts the RTs are independent at a configurable internal
-- resolution while the backbuffer path stays separate. Driven by
-- dx11_eyetargets_test.cpp; verified headless (eye/independence/ires/back
-- probes).
-- ---------------------------------------------------------------------------
project "dx11_eyetargets"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_resources.h",
        "spike/dx11_resources.c",
        "spike/dx11_textures.h",
        "spike/dx11_textures.c",
        "spike/dx11_shaders.h",
        "spike/dx11_shaders.c",
        "spike/dx11_drawcmdexec.h",
        "spike/dx11_drawcmdexec.c",
        "spike/dx11_eyetargets_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 SBS/TB composite (T2.3): fullscreen-triangle pass that samples the two
-- per-eye offscreen SRVs (T2.1) into the backbuffer halves (side-by-side /
-- top-and-bottom), with a swap-eyes option, replacing the legacy GL blit.
-- Driven by dx11_composite_test.cpp on top of the full stack; verified headless
-- (sbs / tb / swap / mono probes).
-- ---------------------------------------------------------------------------
project "dx11_composite"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_resources.h",
        "spike/dx11_resources.c",
        "spike/dx11_textures.h",
        "spike/dx11_textures.c",
        "spike/dx11_shaders.h",
        "spike/dx11_shaders.c",
        "spike/dx11_drawcmdexec.h",
        "spike/dx11_drawcmdexec.c",
        "spike/dx11_composite.h",
        "spike/dx11_composite.c",
        "spike/dx11_composite_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 per-eye projection math (T2.2): game-agnostic stereo camera module
-- (yaw-derived lateral eye offset, per-eye world->view matrix, convergence
-- shear). Driven by dx11_stereo_test.cpp; verified headless (offset / stable /
-- swap / separation / convergence probes). Pure math, no extra links.
-- ---------------------------------------------------------------------------
project "dx11_stereo"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_stereo.h",
        "spike/dx11_stereo.c",
        "spike/dx11_stereo_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 shaders + render state (T1.4): universal VS + flat/gouraud textured PS,
-- the PSX blend/depth-stencil/rasterizer states, and a per-draw flat-color CB.
-- Driven by dx11_shaders_test.cpp on top of dx11_renderer + dx11_textures;
-- verified headless (BMP probe assert of the four modes + AVERAGE blend).
-- ---------------------------------------------------------------------------
project "dx11_shaders"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_textures.h",
        "spike/dx11_textures.c",
        "spike/dx11_shaders.h",
        "spike/dx11_shaders.c",
        "spike/dx11_shaders_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 texture system (T1.3): PSX VRAM staging + tpage/clut/RGB555 decode,
-- baking each paletted region to an R8G8B8A8 SRV (white 1x1 substitute for
-- untextured surfaces). Driven by dx11_textures_test.cpp on top of the
-- dx11_renderer foundation; verified headless (BMP capture + decode assert).
-- ---------------------------------------------------------------------------
project "dx11_textures"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_textures.h",
        "spike/dx11_textures.c",
        "spike/dx11_textures_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"

-- ---------------------------------------------------------------------------
-- DX11 resource management (T1.2): per-frame vertex arena + dynamic VB, a
-- constant-buffer arena for per-draw world matrices, and SRV/sampler binding.
-- Driven by dx11_resources_test.cpp on top of the dx11_renderer foundation;
-- verified headless (BMP capture + bounded-growth stats).
-- ---------------------------------------------------------------------------
project "dx11_resources"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

    files {
        "spike/dx11_renderer.h",
        "spike/dx11_renderer.c",
        "spike/dx11_resources.h",
        "spike/dx11_resources.c",
        "spike/dx11_resources_test.cpp",
    }

    includedirs {
        "spike",
    }

    filter { "files:**.c", "files:**.C" }
        compileas "C++"

    filter { "system:Windows" }
        links {
            "d3d11",
            "dxgi",
            "d3dcompiler",
            "user32",
            "gdi32",
        }

    filter "configurations:Debug"
        targetsuffix "_dbg"
        symbols "On"
        defines { "_DEBUG" }

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Release_dev"
        targetsuffix "_dev"
        optimize "Speed"
        symbols "On"
