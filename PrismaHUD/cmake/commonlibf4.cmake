set(CommonLibF4Path "${CMAKE_SOURCE_DIR}/../../../CommonLibF4")

message(STATUS "Configuring CommonLibF4 from ${CommonLibF4Path}")

if (NOT TARGET CommonLibF4)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CommonLibF4Path}" "${CMAKE_BINARY_DIR}/external_builds/CommonLibF4" EXCLUDE_FROM_ALL)
endif()
