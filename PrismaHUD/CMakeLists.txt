cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(PrismaHUD_F4 VERSION 1.0.0 LANGUAGES CXX)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed.")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CompilerFlags)
include(commonlibf4)

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES ${SOURCES})

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    CommonLibF4::CommonLibF4
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    bcrypt
    version
)

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/PCH.h")

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set(DIST_ROOT "${CMAKE_SOURCE_DIR}/dist")
set(DIST_DIR  "${DIST_ROOT}/PrismaHUD_F4_${PROJECT_VERSION}")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/F4SE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/Data/PrismaUI_F4/views"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${DIST_DIR}/F4SE/Plugins/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_editor.html"      "${DIST_DIR}/Data/PrismaUI_F4/views/hud_editor.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_health.html"      "${DIST_DIR}/Data/PrismaUI_F4/views/hud_health.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_ap.html"          "${DIST_DIR}/Data/PrismaUI_F4/views/hud_ap.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_compass.html"     "${DIST_DIR}/Data/PrismaUI_F4/views/hud_compass.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_rads.html"        "${DIST_DIR}/Data/PrismaUI_F4/views/hud_rads.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_ammo.html"        "${DIST_DIR}/Data/PrismaUI_F4/views/hud_ammo.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_grenade.html"     "${DIST_DIR}/Data/PrismaUI_F4/views/hud_grenade.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_quickloot.html"  "${DIST_DIR}/Data/PrismaUI_F4/views/hud_quickloot.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_survival.html"    "${DIST_DIR}/Data/PrismaUI_F4/views/hud_survival.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/hud_interaction.html" "${DIST_DIR}/Data/PrismaUI_F4/views/hud_interaction.html"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/Data/PrismaUI_F4/views/icons"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/icons" "${DIST_DIR}/Data/PrismaUI_F4/views/icons"
    COMMAND ${CMAKE_COMMAND} -E echo "Distribution ready: dist/PrismaHUD_F4_${PROJECT_VERSION}"
)
