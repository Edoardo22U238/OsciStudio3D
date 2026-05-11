cmake_minimum_required(VERSION 3.22)
project(OsciStudio3D VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Fetch JUCE ────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Fetch Assimp (only needed importers, static) ─────────────
set(BUILD_SHARED_LIBS                     OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS                    OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT                      ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS             OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES                  OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                        OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS             OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_STL_IMPORTER             ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER             ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_PLY_IMPORTER             ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER            ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB                     ON  CACHE BOOL "" FORCE)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.3.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(assimp)

# ── App ───────────────────────────────────────────────────────
juce_add_gui_app(OsciStudio3D
    PRODUCT_NAME     "OsciStudio3D"
    COMPANY_NAME     "OsciStudio"
    VERSION          "1.0.0"
    NEEDS_CURL       FALSE
    NEEDS_WEB_BROWSER FALSE
)

juce_generate_juce_header(OsciStudio3D)

target_sources(OsciStudio3D PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Fonte/Main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Fonte/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Fonte/GeometryProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Fonte/MainComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Fonte/ScopeRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Fonte/WavExporter.cpp
)

target_compile_definitions(OsciStudio3D PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(OsciStudio3D PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /MP /bigobj /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter>
)

target_link_libraries(OsciStudio3D PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    assimp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
