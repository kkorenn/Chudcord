# Native Discord Client (C++)

A high-performance, native Discord client written in C++20 using Boost.Asio, WebSocket++, and Dear ImGui.

## Features

- **High Performance**: Minimal RAM usage (<100MB target), native rendering.
- **Async Networking**: Non-blocking I/O for Gateway and REST API.
- **Custom UI**: Built with Dear ImGui for a responsive, dark-themed interface.
- **Core Discord Functionality**: 
  - Real-time messaging via Gateway.
  - Server and Channel navigation.
  - Message history.
  - Sending messages.

## Prerequisites

### macOS (Homebrew)
```bash
brew install cmake boost openssl curl glfw
```

### Linux (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install build-essential cmake libboost-all-dev libssl-dev libcurl4-openssl-dev libglfw3-dev libgl1-mesa-dev
```

## Build Instructions

1. **Clone the repository** (if applicable) or navigate to the project folder:
   ```bash
   cd native_discord_client
   ```

2. **Create a build directory:**
   ```bash
   mkdir build
   cd build
   ```

3. **Configure with CMake:**
   ```bash
   cmake ..
   ```

4. **Build:**
   ```bash
   make -j$(nproc)
   ```

## Configuration

1. Rename `config.json.example` to `config.json` in the build directory (or where you run the executable).
   ```bash
   cp ../config.json.example config.json
   ```

2. Edit `config.json` and paste your Discord Bot Token (or User Token, though Bot Token is recommended for testing):
   ```json
   {
       "token": "YOUR_TOKEN_HERE"
   }
   ```
   *Note: Using a user token may violate Discord ToS. Proceed with caution.*

## Running

```bash
./discord_client
```

## Architecture

- **Core**: `src/core` - Application loop and state management.
- **Discord**: `src/discord` - Gateway (WebSocket) and REST (HTTP) implementations.
- **UI**: `src/ui` - Rendering logic using Dear ImGui.
