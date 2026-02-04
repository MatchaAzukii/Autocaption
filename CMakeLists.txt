cmake_minimum_required(VERSION 3.20)
project(autocaption VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TESTS "Build tests" OFF)
option(USE_CUDA "Enable CUDA support for Whisper" OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)
elseif(MSVC)
    add_compile_options(/W4 /O2)
endif()

# Find packages
find_package(Threads REQUIRED)

# Platform-specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DPLATFORM_WINDOWS)
    set(PLATFORM_LIBS winmm ole32 oleautimm-dev uuid)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(CORESERVICES_LIBRARY CoreServices)
    set(PLATFORM_LIBS 
        ${COREAUDIO_LIBRARY}
        ${AUDIOUNIT_LIBRARY}
        ${COREFOUNDATION_LIBRARY}
        ${CORESERVICES_LIBRARY}
    )
else()
    add_definitions(-DPLATFORM_LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PULSE libpulse-simple REQUIRED)
    set(PLATFORM_LIBS ${PULSE_LIBRARIES})
    include_directories(${PULSE_INCLUDE_DIRS})
endif()

# Fetch whisper.cpp
include(FetchContent)
FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG v1.6.2
)

set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(USE_CUDA)
    set(WHISPER_CUDA ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(whisper)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${whisper_SOURCE_DIR}
    ${whisper_SOURCE_DIR}/include
    ${whisper_SOURCE_DIR}/ggml/include
)

# Source files
set(CORE_SOURCES
    src/audio/audio_capture.cpp
    src/audio/audio_processor.cpp
    src/audio/ring_buffer.cpp
    src/audio/vad.cpp
    src/asr/whisper_engine.cpp
    src/ipc/json_emitter.cpp
    src/utils/logger.cpp
    src/main.cpp
)

# Platform-specific sources
if(WIN32)
    list(APPEND CORE_SOURCES src/audio/audio_capture_windows.cpp)
elseif(APPLE)
    list(APPEND CORE_SOURCES src/audio/audio_capture_macos.cpp)
else()
    list(APPEND CORE_SOURCES src/audio/audio_capture_linux.cpp)
endif()

# Main executable
add_executable(autocaption ${CORE_SOURCES})

target_link_libraries(autocaption
    PRIVATE
    whisper
    Threads::Threads
    ${PLATFORM_LIBS}
)

if(WIN32)
    target_link_libraries(autocaption PRIVATE ws2_32)
endif()

# WebSocket server for IPC
FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG 0.8.2
)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(websocketpp)

find_package(Boost REQUIRED COMPONENTS system thread)
target_link_libraries(autocaption PRIVATE Boost::system Boost::thread)
target_include_directories(autocaption PRIVATE ${websocketpp_SOURCE_DIR})

# Install
install(TARGETS autocaption DESTINATION bin)
install(DIRECTORY webui/ DESTINATION share/autocaption/webui)

# Print configuration
message(STATUS "")
message(STATUS "Autocaption Configuration:")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  CUDA: ${USE_CUDA}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")
