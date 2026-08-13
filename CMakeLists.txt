cmake_minimum_required(VERSION 3.24)
project(Peek VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Peek WIN32
    src/main.cpp
    src/app.cpp
    src/self_test.cpp
    src/core/event_store.cpp
    src/core/localization.cpp
    src/core/process_resolver.cpp
    src/core/icon_cache.cpp
    src/core/privilege_helper.cpp
    src/core/windows_guids.cpp
    src/ghost/ghost_monitor.cpp
    src/unlock/unlock_service.cpp
    src/focus/focus_inspector.cpp
    src/ui/main_window.cpp
    src/ui/overlay_window.cpp
    resources/peek.rc
)

target_include_directories(Peek PRIVATE src)
target_compile_definitions(Peek PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(Peek PRIVATE /W4 /permissive- /EHsc /utf-8 /Gy /Gw)
    # The application manifest is already embedded by resources/peek.rc.
    target_link_options(Peek PRIVATE /OPT:REF /OPT:ICF /MANIFEST:NO)
    set_property(TARGET Peek PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(Peek PRIVATE -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections)
    target_link_options(Peek PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -municode)
endif()

target_link_libraries(Peek PRIVATE
    comctl32
    dwmapi
    ole32
    oleaut32
    shell32
    shlwapi
    comdlg32
    rstrtmgr
    wbemuuid
    psapi
    advapi32
    gdi32
)

set_target_properties(Peek PROPERTIES
    OUTPUT_NAME "Peek"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
