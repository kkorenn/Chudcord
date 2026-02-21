cmake_minimum_required(VERSION 3.16)
project(chudcord LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix for older subprojects using legacy CMake versions (like websocketpp)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(APPLE)
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# Dependencies via Homebrew
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)

# Boost Detection
find_package(Boost REQUIRED) # Header-only is enough for Asio in many cases

# FetchContent for header-only libraries not in brew or easier to fetch
include(FetchContent)

# 1. websocketpp
FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG 0.8.2
)
FetchContent_MakeAvailable(websocketpp)

# 2. Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# Core Application Sources
file(GLOB_RECURSE CLIENT_SOURCES 
    "src/*.cpp" 
)

add_executable(discord_client 
    ${CLIENT_SOURCES}
    ${IMGUI_SOURCES}
)

# Include Directories
target_include_directories(discord_client PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${websocketpp_SOURCE_DIR}
)

# Link Libraries
target_link_libraries(discord_client PRIVATE
    nlohmann_json::nlohmann_json
    glfw
    OpenGL::GL
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CURL_LIBRARIES}
    ${Boost_LIBRARIES}
    Threads::Threads
)

if(APPLE)
    target_link_libraries(discord_client PRIVATE 
        "-framework Cocoa" 
        "-framework IOKit" 
        "-framework CoreVideo"
        "-framework OpenGL"
    )
endif()

# Compile Definitions
target_compile_definitions(discord_client PRIVATE ASIO_STANDALONE)
