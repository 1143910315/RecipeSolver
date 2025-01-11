add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")
if is_mode("debug") then
    set_runtimes("MDd")
    add_requires("webui", {debug = true})
    add_requires("date", {debug = true})
    add_requires("nlohmann_json", {debug = true})
else
    set_runtimes("MD")
    add_requires("webui")
    add_requires("date")
    add_requires("nlohmann_json")
end
includes("ui/xmake.lua")

target("RecipeSolver")
    add_packages(
        "webui",
        "date",
        "nlohmann_json"
    )
    add_cxflags(
        "cl::/utf-8"
    )
    set_warnings("allextra")
    if is_mode("debug") then
        add_defines("_DEBUG")
        set_symbols("debug", "edit")
    else
        if is_plat("windows") then
            add_ldflags("-subsystem:windows", {force = true})
        end
    end
    set_kind("binary")
    add_deps("ui")
    add_includedirs("src")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    set_languages("clatest", "cxxlatest")