add_requires("spdlog v1.16.0", "fmt", "nlohmann_json")

target("PrismaHUD_F4")
    set_kind("shared")
    set_arch("x64")
    set_filename("PrismaHUD_F4.dll")
    set_version("1.1.1")

    add_deps("commonlibf4")
    add_packages("spdlog", "fmt", "nlohmann_json")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")

    set_pcxxheader("src/PCH.h")

    add_defines("COMMONLIB_RUNTIMECOUNT=3")

    add_syslinks("bcrypt", "version")

    set_targetdir("$(builddir)/windows/x64/release")
