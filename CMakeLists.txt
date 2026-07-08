cmake_minimum_required(VERSION 3.24)

project(Py4GW VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Py4GW currently targets Windows only.")
endif()

if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Py4GW must be configured as a 32-bit build. Use -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Compile translation units in parallel within each target.
    add_compile_options(/MP)
endif()

option(PY4GW_BUILD_IMGUI_DEMO "Compile Dear ImGui demo sources." ON)
option(PY4GW_ENABLE_FREETYPE "Rasterize ImGui fonts with the vendored FreeType (sharper than stb_truetype)." ON)

# Keep the injected DLL beside the runtime payload directories so the module can
# resolve fonts/, scripts/, and offsets/ directly from the repository root.
set(PY4GW_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook)

if(PY4GW_ENABLE_FREETYPE)
    # Vendored FreeType (third_party/freetype), built lean and fully self-contained:
    # every optional external (zlib/bzip2/png/harfbuzz/brotli) is disabled so it pulls
    # in no system library, matching how the rest of third_party/ is vendored. Install
    # rules are suppressed so it never leaks into a Py4GW install.
    set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL    ON CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype)
endif()

set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_dx9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_win32.cpp
)

if(PY4GW_BUILD_IMGUI_DEMO)
    list(APPEND IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp)
endif()

if(PY4GW_ENABLE_FREETYPE)
    list(APPEND IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/freetype/imgui_freetype.cpp)
endif()

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
)
# Do NOT define IMGUI_DISABLE_OBSOLETE_FUNCTIONS for a subset of targets: it
# changes sizeof(ImGuiIO), and any TU compiled with a different setting than
# imgui.cpp fails IMGUI_CHECKVERSION ("Mismatched struct layout!") in Debug.
# It stays off entirely because third_party addons (ImAnim) still use
# obsolete ImGui APIs.
target_compile_definitions(imgui
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)
target_link_libraries(imgui PUBLIC d3d9.lib)

if(PY4GW_ENABLE_FREETYPE)
    # Compile imgui_draw.cpp + imgui_freetype.cpp with IMGUI_ENABLE_FREETYPE so the
    # atlas auto-installs the FreeType loader (imgui_draw.cpp: SetFontLoader on first
    # build). PRIVATE is sufficient: no consumer TU references FreeType directly, and
    # the define does not affect any public ImGui struct layout. freetype's include
    # dirs arrive transitively via its PUBLIC interface.
    target_link_libraries(imgui PUBLIC freetype)
    target_compile_definitions(imgui PRIVATE IMGUI_ENABLE_FREETYPE)
endif()

file(GLOB_RECURSE PY4GW_IMGUI_ADDON_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui/addons/*.cpp
)

set(IMGUI_ADDON_SOURCES
    ${PY4GW_IMGUI_ADDON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui-filebrowser/ImGuiFileBrowser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImAnim/im_anim.cpp
)

add_library(imgui_addons STATIC ${IMGUI_ADDON_SOURCES})
target_include_directories(imgui_addons
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui-filebrowser
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImHotKey
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui_markdown
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImAnim
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui_club/imgui_memory_editor
)
target_compile_definitions(imgui_addons
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
)
target_compile_options(imgui_addons PRIVATE /FIbase/error_handling.h)
target_link_libraries(imgui_addons PUBLIC imgui d3d9.lib)

file(GLOB_RECURSE PY4GW_ALL_SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

set(PY4GW_SOURCES ${PY4GW_ALL_SOURCE_FILES})
list(REMOVE_ITEM PY4GW_SOURCES ${PY4GW_IMGUI_ADDON_SOURCES})

file(GLOB_RECURSE PY4GW_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IMGUI_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PY4GW_IMGUI_ADDON_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PY4GW_ALL_SOURCE_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PY4GW_HEADERS})

add_library(Py4GW SHARED ${PY4GW_SOURCES} ${PY4GW_HEADERS})

set_target_properties(Py4GW PROPERTIES
    OUTPUT_NAME "Py4GW"
    PREFIX ""
)

target_include_directories(Py4GW
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui-filebrowser
)

target_compile_definitions(Py4GW
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        PYBIND11_DETAILED_ERROR_MESSAGES
)
target_compile_options(Py4GW PRIVATE /FIbase/error_handling.h)

target_link_libraries(Py4GW
    PRIVATE
        pybind11::embed
        imgui
        imgui_addons
        minhook
        Python3::Python
        d3d9.lib
        d3dcompiler.lib
        dbghelp.lib
        winmm.lib
        windowscodecs.lib
        ole32.lib
        gdiplus.lib
        comctl32.lib
)

