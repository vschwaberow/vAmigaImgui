<p align="center">
  <div><a href = "https://dirkwhoffmann.github.io/vAmiga"><img src="https://dirkwhoffmann.github.io/vAmiga/images/redirect4.png"></a></div>
  <div><a href = "https://vamiganet.github.io"><img src="https://dirkwhoffmann.github.io/vAmiga/images/redirect5.png"></a></div>
</p>

## Linux / ImGui Port

This repository includes an experimental **Linux port** of vAmiga, utilizing **SDL2** and **Dear ImGui** to provide a cross-platform graphical user interface.

While the core emulation remains identical to the macOS version, the frontend has been rewritten to support Linux desktop environments. Key features of this port include:

-   **Native Linux Support:** Builds and runs on modern Linux distributions (Ubuntu, Fedora, Arch, etc.).
-   **MacOS Support:** Builds and run on MacOS
-   **ImGui Interface:** A completely new, lightweight, and functional UI for configuring the emulator, managing media, and inspecting the system state.
-   **Hardware Acceleration:** Uses OpenGL 3.0+ for rendering.
-   **Input Support:** Robust keyboard and mouse integration, including mouse capture/release modes and full support for **physical gamepads** (via SDL2).
-   **Tools:** Includes ported versions of the Inspector, Dashboard, and Console for debugging and monitoring.

### Building for Linux

Requirements: `cmake`, `gcc` (or `clang`), `libsdl2-dev`.

```bash
# 1. Clone the repository
git clone https://github.com/vschwaberow/vAmigaImgui.git
cd vAmiga

# 2. Fetch submodules
git submodule update --init --recursive

# 3. Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 4. Build
cmake --build build -j$(nproc)

# 5. Run
./build/bin/vAmigaImgui
```

#### Offline or pre-downloaded dependencies

If you cannot use network access during configuration, point CMake at local copies of the dependencies:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DVAMIGA_CORE_DIR=/path/to/vAmiga/Core \
  -DIMGUI_DIR=/path/to/imgui \
  -DIMGUIFILEDIALOG_DIR=/path/to/ImGuiFileDialog \
  -DENABLE_TESTS=OFF
```

If you want tests on, also add `-DGTEST_DIR=/path/to/googletest`.

## MCP Server Integration

This port includes an embedded Model Context Protocol (MCP) server, allowing AI assistants (like Claude) to directly interact with the Amiga emulator (e.g., read screen states, memory dumps, and trigger emulator features).

### Claude Desktop Configuration

Claude Desktop expects an MCP server to communicate via standard input/output (`stdio`). Because `vAmigaImgui` is a graphical application that outputs console logs, you must use the provided Python bridge script to route the JSON-RPC traffic safely over TCP.

1. Start `vAmigaImgui`.
2. Press `F12` to open Settings, go to the **MCP Server** tab, check **Enable MCP TCP Server**, and ensure the Port is `8080`.
3. Add the following to your `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "vAmiga-MCP": {
      "command": "python3",
      "args": [
        "/absolute/path/to/vAmigaImgui/scripts/mcp_stdio_bridge.py",
        "127.0.0.1",
        "8080"
      ]
    }
  }
}
```

*Note: Replace `/absolute/path/to/vAmigaImgui` with the actual path to your clone.*

### Cursor / Codex Configuration

In Cursor or other Codex-powered editor clients that support MCP, you can add the server directly via the editor's MCP settings:

1. **Type:** `command` (or `stdio`)
2. **Command:** `python3`
3. **Arguments:** `/absolute/path/to/vAmigaImgui/scripts/mcp_stdio_bridge.py 127.0.0.1 8080`

### Gemini CLI

For the Gemini CLI tool, you can register the MCP server in your tools configuration file (often `mcp.json` or passed via command line). The structure is identical to the standard MCP definition:

```json
{
  "mcpServers": {
    "vAmiga-MCP": {
      "command": "python3",
      "args": [
        "/absolute/path/to/vAmigaImgui/scripts/mcp_stdio_bridge.py",
        "127.0.0.1",
        "8080"
      ]
    }
  }
}
```

### OpenCode

In OpenCode, configure the MCP tool integration by providing the Python bridge script as the executable tool source:

```json
{
  "mcpServers": {
    "vAmiga-MCP": {
      "command": "python3",
      "args": [
        "/absolute/path/to/vAmigaImgui/scripts/mcp_stdio_bridge.py",
        "127.0.0.1",
        "8080"
      ]
    }
  }
}
```
