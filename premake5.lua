-- premake5.lua
-- version: premake-5.0.0-alpha14

newoption {
    trigger     = "clang",
    description = "Force use of CLANG for Windows builds"
}

function win_copy_to_plugins(from)
    return 'XCOPY "' .. from:gsub("/", "\\") .. '" "C:\\work\\bin\\debug\\plugins\\" /Q/Y/I/C '
end

workspace "tm_zig"
    configurations {"Debug", "Release"}
    language "C++"
    cppdialect "C++11"
    flags { "FatalWarnings", "MultiProcessorCompile" }
    warnings "Extra"
    inlining "Auto"
    sysincludedirs { "" }
    targetdir "."

filter "system:windows"
    platforms { "Win64" }
    systemversion("latest")

filter { "system:windows", "options:clang" }
    toolset("msc-clangcl")
    buildoptions {
        "-Wno-missing-field-initializers",   -- = {0} is OK.
        "-Wno-unused-parameter",             -- Useful for documentation purposes.
        "-Wno-unused-local-typedef",         -- We don't always use all typedefs.
        "-Wno-missing-braces",               -- = {0} is OK.
        "-Wno-microsoft-anon-tag",           -- Allow anonymous structs.
    }
    buildoptions {
        "-fms-extensions",                   -- Allow anonymous struct as C inheritance.
        "-mavx",                             -- AVX.
        "-mfma",                             -- FMA.
    }
    removeflags {"FatalLinkWarnings"}        -- clang linker doesn't understand /WX

filter "platforms:Win64"
    defines { "TM_OS_WINDOWS", "_CRT_SECURE_NO_WARNINGS" }
    includedirs { "C:/Work/themachinery" }
    staticruntime "On"
    architecture "x64"
    disablewarnings {
        "4057", -- Slightly different base types. Converting from type with volatile to without.
        "4100", -- Unused formal parameter. I think unusued parameters are good for documentation.
        "4152", -- Conversion from function pointer to void *. Should be ok.
        "4200", -- Zero-sized array. Valid C99.
        "4201", -- Nameless struct/union. Valid C11.
        "4204", -- Non-constant aggregate initializer. Valid C99.
        "4206", -- Translation unit is empty. Might be #ifdefed out.
        "4214", -- Bool bit-fields. Valid C99.
        "4221", -- Pointers to locals in initializers. Valid C99.
        "4702", -- Unreachable code. We sometimes want return after exit() because otherwise we get an error about no return value.
    }
    linkoptions {"/ignore:4099"}

filter "configurations:Debug"
    defines { "TM_CONFIGURATION_DEBUG", "DEBUG" }
    symbols "On"

filter "configurations:Release"
    defines { "TM_CONFIGURATION_RELEASE" }
    optimize "On"

project "zig"
    location "build/zig"
    targetname "tm_zig"
    kind "SharedLib"
    language "C++"
    files {"*.inl", "*.h", "*.c"}
    sysincludedirs { "" }
    postbuildcommands {
        "{COPY} %{wks.location}/tm_zig.dll C:/work/themachinery/bin/debug/plugins",
        "{COPY} %{wks.location}/tm_zig.pdb C:/work/themachinery/bin/debug/plugins",

    }
