#include "services/mcp_server.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <print>
#include <array>
#include <format>
#include <sstream>
#include <nlohmann/json.hpp>
#include <span>
#include <ranges>
#include <algorithm>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#include "Components/Memory/MemoryTypes.h"
#include "Components/CPU/CPU.h"
#include "Components/Memory/Memory.h"

#include <unordered_map>
#include <functional>

using json = nlohmann::json;

namespace gui {

McpServer::McpServer(vamiga::VAmiga& emulator) : emulator_(emulator) {}

McpServer::~McpServer() {
  Stop();
}

std::string McpServer::Base64Encode(const uint8_t* data, size_t length) {
  static const char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t output_length = 4 * ((length + 2) / 3);
  std::string encoded;
  encoded.reserve(output_length);

  for (size_t i = 0; i < length; ) {
    uint32_t octet_a = i < length ? data[i++] : 0;
    uint32_t octet_b = i < length ? data[i++] : 0;
    uint32_t octet_c = i < length ? data[i++] : 0;
    // Note: if i >= length, i must still be forced to increment so the loop terminates
    if (i >= length && octet_a == 0 && octet_b == 0 && octet_c == 0) break;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded.push_back(encoding_table[(triple >> 3 * 6) & 0x3F]);
    encoded.push_back(encoding_table[(triple >> 2 * 6) & 0x3F]);
    encoded.push_back(encoding_table[(triple >> 1 * 6) & 0x3F]);
    encoded.push_back(encoding_table[(triple >> 0 * 6) & 0x3F]);
  }

  static constexpr std::array<int, 3> mod_table = {0, 2, 1};
  for (int i : std::views::iota(0, mod_table[length % 3])) {
    encoded[encoded.length() - 1 - i] = '=';
  }

  return encoded;
}

void McpServer::Start(const std::string& host, int port) {
  if (running_) {
    if (port_ == port && host_ == host) return;
    Stop();
  }

  host_ = host;
  port_ = port;
  running_ = true;
  server_thread_ = std::thread(&McpServer::RunServer, this);
  notification_thread_ = std::thread(&McpServer::RunNotifications, this);
}

void McpServer::Stop() {
  if (!running_) return;

  running_ = false;
  
  if (server_socket_ != -1) {
    shutdown(server_socket_, SHUT_RDWR);
    close(server_socket_);
    server_socket_ = -1;
  }

  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int sock : active_sockets_) {
      shutdown(sock, SHUT_RDWR);
      close(sock);
    }
  }

  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  if (notification_thread_.joinable()) {
    notification_thread_.join();
  }

  while (active_clients_ > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void McpServer::RunServer() {
  server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket_ == -1) {
    std::println(stderr, "McpServer Error: Failed to create socket");
    running_ = false;
    return;
  }

  int opt = 1;
  if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    std::println(stderr, "McpServer Error: setsockopt failed");
  }

  struct sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port_);

  if (host_.empty() || host_ == "0.0.0.0" || host_ == "*") {
    address.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_pton(AF_INET, host_.c_str(), &address.sin_addr) <= 0) {
      std::println(stderr, "McpServer Error: Invalid host IP {}, defaulting to INADDR_ANY", host_);
      address.sin_addr.s_addr = INADDR_ANY;
    }
  }

  if (bind(server_socket_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
    std::println(stderr, "McpServer Error: Bind failed on {}:{}", host_, port_);
    close(server_socket_);
    running_ = false;
    return;
  }

  if (listen(server_socket_, 3) < 0) {
    std::println(stderr, "McpServer Error: Listen failed");
    close(server_socket_);
    running_ = false;
    return;
  }

  std::println("McpServer listening on port {}", port_);

  while (running_) {
    struct sockaddr_in client_address{};
    socklen_t client_len = sizeof(client_address);
    int client_socket = accept(server_socket_, (struct sockaddr*)&client_address, &client_len);

    if (client_socket < 0) {
      if (running_) {
        std::println(stderr, "McpServer Error: Accept failed");
      }
      continue;
    }

    active_clients_++;
    std::thread(&McpServer::HandleClient, this, client_socket).detach();
  }
}

void McpServer::RunNotifications() {
  bool was_paused = emulator_.isPaused();

  while (running_) {
    bool is_paused = emulator_.isPaused();
    
    if (is_paused && !was_paused) {
      json notification = {
          {"jsonrpc", "2.0"},
          {"method", "notifications/message"},
          {"params", {
              {"level", "info"},
              {"logger", "vamiga"},
              {"data", {
                  {"event", "emulator_paused"},
                  {"pc", emulator_.cpu.getInfo().pc0}
              }}
          }}
      };
      
      std::string msg = notification.dump() + "\n";
      
      std::lock_guard<std::mutex> lock(clients_mutex_);
      for (int sock : active_sockets_) {
        send(sock, msg.data(), msg.length(), MSG_NOSIGNAL);
      }
    }
    
    was_paused = is_paused;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

void McpServer::HandleClient(int client_socket) {
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    active_sockets_.insert(client_socket);
  }

  constexpr size_t kReceiveBufferSize = 4096;
  std::array<char, kReceiveBufferSize> buffer{};
  std::string accumulated_data;
  bool running_client = true;

  while (running_ && running_client) {
    int valread = read(client_socket, buffer.data(), buffer.size() - 1);
    if (valread <= 0) {
      break; 
    }
    
    if (accumulated_data.size() + valread > 10 * 1024 * 1024) {
      break; 
    }
    accumulated_data.append(buffer.data(), valread);

    size_t consumed = 0;
    size_t brace_count = 0;
    bool in_string = false;
    bool escape = false;
    size_t start_idx = 0;

    size_t i = 0;
    for (const char c : accumulated_data) {
      if (c == '"' && !escape) in_string = !in_string;
      else if (c == '\\' && !escape) escape = true;
      else escape = false;

      if (!in_string) {
        if (c == '{') {
          if (brace_count == 0) start_idx = i;
          brace_count++;
        } else if (c == '}') {
          brace_count--;
          if (brace_count == 0) {
            std::string msg = accumulated_data.substr(start_idx, i - start_idx + 1);
            std::string response;
            try {
              response = ProcessMessage(msg);
            } catch (const std::exception& e) {
              response = CreateErrorResponse(json(nullptr), -32603, std::string("Internal error: ") + e.what());
            } catch (...) {
              response = CreateErrorResponse(json(nullptr), -32603, "Unknown internal error");
            }
            if (response == "__EXIT__") {
              running_client = false;
              break;
            }
            if (!response.empty()) {
              send(client_socket, response.data(), response.length(), MSG_NOSIGNAL);
              send(client_socket, "\n", 1, MSG_NOSIGNAL);
            }
            consumed = i + 1;
          }
        }
      }
      i++;
    }

    if (consumed > 0) {
      accumulated_data.erase(0, consumed);
    }
  }

  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    active_sockets_.erase(client_socket);
  }
  close(client_socket);
  active_clients_--;
}

std::string McpServer::ProcessMessage(const std::string& message) {
  try {
    json req = json::parse(message);
    if (!req.contains("method")) return "";

    const std::string method = req["method"];
    
    using HandlerFunc = std::string (McpServer::*)(const std::string&);
    static const std::unordered_map<std::string, HandlerFunc> method_handlers = {
      {"initialize", &McpServer::HandleInitialize},
      {"mcp/server/tools/list", &McpServer::HandleListTools},
      {"tools/list", &McpServer::HandleListTools},
      {"mcp/server/tools/call", &McpServer::HandleCallTool},
      {"tools/call", &McpServer::HandleCallTool},
      {"shutdown", &McpServer::HandleShutdown},
      {"exit", &McpServer::HandleExit},
      {"$/cancelRequest", &McpServer::HandleCancelRequest},
      {"resources/list", &McpServer::HandleListResources},
      {"resources/read", &McpServer::HandleReadResource},
      {"prompts/list", &McpServer::HandleListPrompts},
      {"prompts/get", &McpServer::HandleGetPrompt},
      {"notifications/initialized", &McpServer::HandleInitializedNotification}
    };

    auto it = method_handlers.find(method);
    if (it != method_handlers.end()) {
      return (this->*(it->second))(message);
    }

    if (method == "ping") {
       return CreateSuccessResponse(req.value("id", json(nullptr)), json::object());
    }
    
    return CreateErrorResponse(req.value("id", json(nullptr)), -32601, "Method not found");
  } catch (const json::exception& e) {
    return CreateErrorResponse(json(nullptr), -32700, std::string("Parse or type error: ") + e.what());
  }
}

std::string McpServer::HandleInitialize(const std::string& json_req) {
  json req = json::parse(json_req);
  json result = {
    {"protocolVersion", "2024-11-05"},
    {"capabilities", {
      {"tools", {{}}}
    }},
    {"serverInfo", {
      {"name", "vAmigaImgui-MCP"},
      {"version", "1.0.0"}
    }}
  };
  return CreateSuccessResponse(req.value("id", json(nullptr)), result);
}

std::string McpServer::HandleListTools(const std::string& json_req) {
  json req = json::parse(json_req);
  static const json tools_list = {
    {
      {"name", "get_status"},
      {"description", "Get the current running status of the emulator."},
      {"inputSchema", {{"type", "object"}, {"properties", {}}}}
    },
    {
      {"name", "pause"},
      {"description", "Pause the emulator execution."},
      {"inputSchema", {{"type", "object"}, {"properties", {}}}}
    },
    {
      {"name", "resume"},
      {"description", "Resume the emulator execution."},
      {"inputSchema", {{"type", "object"}, {"properties", {}}}}
    },
    {
      {"name", "hard_reset"},
      {"description", "Perform a hard reset initialized by the machine."},
      {"inputSchema", {{"type", "object"}, {"properties", {}}}}
    },
    {
      {"name", "read_memory"},
      {"description", "Reads bytes from memory as a hex dump."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"address", {{"type", "integer"}}},
          {"size", {{"type", "integer"}}}
        }},
        {"required", {"address", "size"}}
      }}
    },
    {
      {"name", "write_memory"},
      {"description", "Writes an array of bytes to the specified memory address."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"address", {{"type", "integer"}}},
          {"data", {
            {"type", "array"},
            {"items", {{"type", "integer"}}}
          }}
        }},
        {"required", {"address", "data"}}
      }}
    },
    {
      {"name", "get_registers"},
      {"description", "Returns the state of CPU registers and Amiga custom chips."},
      {"inputSchema", {{"type", "object"}, {"properties", {}}}}
    },
    {
      {"name", "set_register"},
      {"description", "Sets a specific CPU register (e.g., PC, D0-D7, A0-A7)."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"name", {{"type", "string"}}},
          {"value", {{"type", "integer"}}}
        }},
        {"required", {"name", "value"}}
      }}
    },
    {
      {"name", "set_breakpoint"},
      {"description", "Sets an execution breakpoint at the target address."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"address", {{"type", "integer"}}}
        }},
        {"required", {"address"}}
      }}
    },
    {
      {"name", "remove_breakpoint"},
      {"description", "Removes an execution breakpoint at the target address."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"address", {{"type", "integer"}}}
        }},
        {"required", {"address"}}
      }}
    },
    {
      {"name", "set_watchpoint"},
      {"description", "Sets a memory watchpoint at the target address."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"address", {{"type", "integer"}}}
        }},
        {"required", {"address"}}
      }}
    },
    {
      {"name", "remove_watchpoint"},
      {"description", "Removes a memory watchpoint at the target address."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"address", {{"type", "integer"}}}
        }},
        {"required", {"address"}}
      }}
    },
    {
      {"name", "list_guards"},
      {"description", "Returns a list of all active breakpoints and watchpoints."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {}},
        {"required", {}}
      }}
    },
    {
      {"name", "disassemble"},
      {"description", "Disassembles 68000 machine code instructions from memory."},
      {"inputSchema", {
        {"type", "object"},
        {"properties", {
          {"address", {{"type", "integer"}}},
          {"count", {{"type", "integer"}}}
        }},
        {"required", {"address", "count"}}
      }}
    }
  };

  return CreateSuccessResponse(req.value("id", json(nullptr)), {{"tools", tools_list}});
}

std::string McpServer::HandleCallTool(const std::string& json_req) {
  json req = json::parse(json_req);
  std::string text_result;
  bool is_error = false;
  json json_result = nullptr;

  using ToolHandler = std::function<void(McpServer*, const json&, std::string&, bool&, json&)>;
  static const std::unordered_map<std::string, ToolHandler> tool_handlers = {
    {"get_status", [](McpServer* s, const json&, std::string& text_res, bool&, json&) {
      text_res = std::string("Emulator is ") + (s->emulator_.isRunning() ? "running" : "paused") + ". Powered on: " + (s->emulator_.isPoweredOn() ? "yes" : "no");
    }},
    {"pause", [](McpServer* s, const json&, std::string& text_res, bool&, json&) {
      s->emulator_.pause();
      text_res = "Emulator paused.";
    }},
    {"resume", [](McpServer* s, const json&, std::string& text_res, bool&, json&) {
      s->emulator_.run();
      text_res = "Emulator resumed.";
    }},
    {"hard_reset", [](McpServer* s, const json&, std::string& text_res, bool&, json&) {
      s->emulator_.hardReset();
      text_res = "Emulator hard reset triggered.";
    }},
    {"read_memory", [](McpServer* s, const json& r, std::string& text_res, bool& err, json&) {
      uint32_t addr = r.at("params").at("arguments").at("address").get<uint32_t>();
      int size = r.at("params").at("arguments").at("size").get<int>();
      constexpr uint32_t kMaxAmigaAddress = 0x1000000;
      constexpr int kMaxPayloadSize = 65536;
      if (size < 0 || size > kMaxPayloadSize || addr > kMaxAmigaAddress || (kMaxAmigaAddress - addr) < static_cast<uint32_t>(size)) {
        err = true;
        text_res = "Invalid address or size out of bounds (max size 64KB).";
      } else {
        text_res = s->emulator_.mem.debugger.hexDump(vamiga::Accessor::CPU, addr, size);
      }
    }},
    {"write_memory", [](McpServer* s, const json& r, std::string& text_res, bool& err, json&) {
      uint32_t addr = r.at("params").at("arguments").at("address").get<uint32_t>();
      const auto& data = r.at("params").at("arguments").at("data");
      constexpr uint32_t kMaxAmigaAddress = 0x1000000;
      constexpr size_t kMaxPayloadSize = 65536;
      if (!data.is_array() || data.size() > kMaxPayloadSize || addr > kMaxAmigaAddress || (kMaxAmigaAddress - addr) < data.size()) {
        err = true;
        text_res = "Invalid address, data format, or sizes out of bounds (max 64KB).";
      } else {
        uint32_t offset = 0;
        for (const auto& byte : data) {
          s->emulator_.mem.mem->poke8<vamiga::Accessor::CPU>(addr + offset++, byte.get<uint8_t>());
        }
        text_res = "Memory written successfully.";
      }
    }},
    {"get_registers", [](McpServer* s, const json&, std::string&, bool&, json& json_res) {
      auto cpu_info = s->emulator_.cpu.getInfo();
      auto agnus_info = s->emulator_.agnus.getInfo();
      auto paula_info = s->emulator_.paula.getInfo();
      json_res = {
        {"CPU", {
          {"PC", cpu_info.pc0}, {"SR", cpu_info.sr}, {"USP", cpu_info.usp},
          {"ISP", cpu_info.isp}, {"MSP", cpu_info.msp},
          {"D", {cpu_info.d[0], cpu_info.d[1], cpu_info.d[2], cpu_info.d[3], cpu_info.d[4], cpu_info.d[5], cpu_info.d[6], cpu_info.d[7]}},
          {"A", {cpu_info.a[0], cpu_info.a[1], cpu_info.a[2], cpu_info.a[3], cpu_info.a[4], cpu_info.a[5], cpu_info.a[6], cpu_info.a[7]}}
        }},
        {"Agnus", {{"DMACON", agnus_info.dmacon}, {"VPOS", agnus_info.vpos}, {"HPOS", agnus_info.hpos}}},
        {"Paula", {{"INTREQ", paula_info.intreq}, {"INTENA", paula_info.intena}}}
      };
    }},
    {"set_register", [](McpServer* s, const json& r, std::string& text_res, bool& err, json&) {
      std::string reg_name = r.at("params").at("arguments").at("name").get<std::string>();
      uint32_t val = r.at("params").at("arguments").at("value").get<uint32_t>();
      if (reg_name == "PC") s->emulator_.cpu.cpu->setPC(val);
      else if (reg_name == "USP") s->emulator_.cpu.cpu->setUSP(val);
      else if (reg_name == "ISP") s->emulator_.cpu.cpu->setISP(val);
      else if (reg_name == "MSP") s->emulator_.cpu.cpu->setMSP(val);
      else if (reg_name == "SR") s->emulator_.cpu.cpu->setSR(val);
      else if (reg_name.length() == 2 && reg_name[0] == 'D' && reg_name[1] >= '0' && reg_name[1] <= '7') {
        s->emulator_.cpu.cpu->setD(reg_name[1] - '0', val);
      } else if (reg_name.length() == 2 && reg_name[0] == 'A' && reg_name[1] >= '0' && reg_name[1] <= '7') {
        s->emulator_.cpu.cpu->setA(reg_name[1] - '0', val);
      } else {
        err = true;
        text_res = "Invalid register name.";
      }
      if (!err) text_res = "Register updated.";
    }},
    {"set_breakpoint", [](McpServer* s, const json& r, std::string& text_res, bool&, json&) {
      uint32_t addr = r.at("params").at("arguments").at("address").get<uint32_t>();
      s->emulator_.cpu.breakpoints.setAt(addr);
      text_res = "Breakpoint set.";
    }},
    {"remove_breakpoint", [](McpServer* s, const json& r, std::string& text_res, bool&, json&) {
      uint32_t addr = r.at("params").at("arguments").at("address").get<uint32_t>();
      s->emulator_.cpu.breakpoints.removeAt(addr);
      text_res = "Breakpoint removed.";
    }},
    {"set_watchpoint", [](McpServer* s, const json& r, std::string& text_res, bool&, json&) {
      uint32_t addr = r.at("params").at("arguments").at("address").get<uint32_t>();
      s->emulator_.cpu.watchpoints.setAt(addr);
      text_res = "Watchpoint set.";
    }},
    {"remove_watchpoint", [](McpServer* s, const json& r, std::string& text_res, bool&, json&) {
      uint32_t addr = r.at("params").at("arguments").at("address").get<uint32_t>();
      s->emulator_.cpu.watchpoints.removeAt(addr);
      text_res = "Watchpoint removed.";
    }},
    {"list_guards", [](McpServer* s, const json&, std::string&, bool&, json& json_res) {
      json breakpoints = json::array();
      for (int i = 0; i < s->emulator_.cpu.breakpoints.elements(); ++i) {
        if (auto info = s->emulator_.cpu.breakpoints.guardNr(i)) {
          breakpoints.push_back({{"address", info->addr}, {"enabled", info->enabled}});
        }
      }
      json watchpoints = json::array();
      for (int i = 0; i < s->emulator_.cpu.watchpoints.elements(); ++i) {
        if (auto info = s->emulator_.cpu.watchpoints.guardNr(i)) {
          watchpoints.push_back({{"address", info->addr}, {"enabled", info->enabled}});
        }
      }
      json_res["breakpoints"] = breakpoints;
      json_res["watchpoints"] = watchpoints;
    }},
    {"disassemble", [](McpServer* s, const json& r, std::string& text_res, bool& err, json&) {
      uint32_t addr = r.at("params").at("arguments").at("address").get<uint32_t>();
      int count = r.at("params").at("arguments").at("count").get<int>();
      
      if (count <= 0 || count > 10000) {
        err = true;
        text_res = "Invalid instruction count (max 10000)";
        return;
      }
      
      std::stringstream ss;
      if (!s->emulator_.cpu.cpu) {
         text_res = "CPU not initialized (dummy mode)";
         return;
      }
      for (auto _ : std::views::iota(0, count)) {
        vamiga::isize len = 0;
        std::string instr = s->emulator_.cpu.debugger.disassembleInstr(addr, &len);
        ss << std::format("{:08X}    {}\n", addr, instr);
        if (len <= 0) len = 2;
        addr += len;
      }
      text_res = ss.str();
    }}
  };

  try {
    std::string tool_name = req.at("params").at("name").get<std::string>();
    auto it = tool_handlers.find(tool_name);
    if (it != tool_handlers.end()) {
      it->second(this, req, text_result, is_error, json_result);
    } else {
      is_error = true;
      text_result = "Tool not found.";
    }
  } catch (const std::exception& e) {
    is_error = true;
    text_result = std::string("Error executing tool (invalid parameters?): ") + e.what();
  }

  json result_content;
  if (!json_result.is_null()) {
    result_content = {
      {"type", "text"},
      {"text", json_result.dump(2)}
    };
  } else {
    result_content = {
      {"type", "text"},
      {"text", text_result}
    };
  }

  return CreateSuccessResponse(req["id"], {
    {"content", {result_content}},
    {"isError", is_error}
  });
}

std::string McpServer::CreateErrorResponse(const json& id_val, int code, const std::string& msg) {
  json res = {
    {"jsonrpc", "2.0"},
    {"id", id_val},
    {"error", {
      {"code", code},
      {"message", msg}
    }}
  };
  return res.dump();
}

std::string McpServer::CreateSuccessResponse(const json& id_val, const json& result_val) {
  json res = {
    {"jsonrpc", "2.0"},
    {"id", id_val},
    {"result", result_val}
  };
  return res.dump();
}

std::string McpServer::HandleShutdown(const std::string& json_req) {
  json req;
  try { req = json::parse(json_req); } catch (...) { return ""; }
  return CreateSuccessResponse(req.value("id", json(nullptr)), json::object());
}

std::string McpServer::HandleExit(const std::string&) {
  return "__EXIT__";
}

std::string McpServer::HandleCancelRequest(const std::string&) {
  return ""; 
}

std::string McpServer::HandleInitializedNotification(const std::string&) {
  return ""; 
}

std::string McpServer::HandleListResources(const std::string& json_req) {
  json req = json::parse(json_req);
  json resources = {
    {
      {"uri", "amiga://screen/current"},
      {"name", "Current Screen Buffer"},
      {"description", "A raw dump of the Amiga screen buffer or information"},
      {"mimeType", "text/plain"}
    },
    {
      {"uri", "amiga://memory/dump"},
      {"name", "Full Memory Dump"},
      {"description", "A dump of the entire 16MB Amiga memory space"},
      {"mimeType", "application/octet-stream"}
    }
  };
  return CreateSuccessResponse(req.value("id", json(nullptr)), {{"resources", resources}});
}

std::string McpServer::HandleReadResource(const std::string& json_req) {
  json req = json::parse(json_req);
  std::string uri = req["params"]["uri"];

  if (uri == "amiga://screen/current") {
    int w = vamiga::HPIXELS;
    int h = vamiga::VPIXELS;
    
    std::vector<uint8_t> bmp_data;
    size_t pixel_data_size = w * h * 4;
    size_t file_size = 54 + pixel_data_size;
    bmp_data.reserve(file_size);
    
    bmp_data.push_back('B'); bmp_data.push_back('M');
    bmp_data.push_back(file_size & 0xFF);
    bmp_data.push_back((file_size >> 8) & 0xFF);
    bmp_data.push_back((file_size >> 16) & 0xFF);
    bmp_data.push_back((file_size >> 24) & 0xFF);
    bmp_data.push_back(0); bmp_data.push_back(0);
    bmp_data.push_back(0); bmp_data.push_back(0);
    bmp_data.push_back(54); bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0);

    bmp_data.push_back(40); bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0);
    bmp_data.push_back(w & 0xFF); bmp_data.push_back((w >> 8) & 0xFF); bmp_data.push_back((w >> 16) & 0xFF); bmp_data.push_back((w >> 24) & 0xFF);
    
    int32_t height_neg = -h;
    uint32_t hn = static_cast<uint32_t>(height_neg);
    bmp_data.push_back(hn & 0xFF); bmp_data.push_back((hn >> 8) & 0xFF); bmp_data.push_back((hn >> 16) & 0xFF); bmp_data.push_back((hn >> 24) & 0xFF);
    
    bmp_data.push_back(1); bmp_data.push_back(0);
    bmp_data.push_back(32); bmp_data.push_back(0);
    bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0);
    
    bmp_data.push_back(pixel_data_size & 0xFF); bmp_data.push_back((pixel_data_size >> 8) & 0xFF); 
    bmp_data.push_back((pixel_data_size >> 16) & 0xFF); bmp_data.push_back((pixel_data_size >> 24) & 0xFF);
    
    bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0);
    bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0);
    bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0);
    bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0); bmp_data.push_back(0);

    const uint32_t* src_pixels = nullptr;
    if (emulator_.cpu.cpu) {
        src_pixels = reinterpret_cast<const uint32_t*>(emulator_.videoPort.getTexture());
    }
    
    if (!src_pixels) {
        bmp_data.insert(bmp_data.end(), w * h * 4, 0);
    } else {
        for (uint32_t px : std::span{src_pixels, static_cast<size_t>(w * h)}) {
            uint8_t r = (px >> 24) & 0xFF;
            uint8_t g = (px >> 16) & 0xFF;
            uint8_t b = (px >> 8) & 0xFF;
            uint8_t a = px & 0xFF;
            
            bmp_data.push_back(b);
            bmp_data.push_back(g);
            bmp_data.push_back(r);
            bmp_data.push_back(a);
        }
    }
    
    std::string base64_image = Base64Encode(bmp_data.data(), bmp_data.size());

    return CreateSuccessResponse(req.value("id", json(nullptr)), {
      {"contents", {
        {
          {"uri", uri},
          {"mimeType", "image/bmp"},
          {"text", base64_image}
        }
      }}
    });
  } else if (uri == "amiga://memory/dump") {
    constexpr uint32_t kDumpSize = 4 * 1024 * 1024;
    std::vector<uint8_t> dump_data(kDumpSize, 0);

    if (emulator_.mem.mem) {
      bool was_running = emulator_.isRunning();
      if (was_running) emulator_.pause();

      for (uint32_t addr : std::views::iota(0u, kDumpSize)) {
        dump_data[addr] = emulator_.mem.mem->spypeek8<vamiga::Accessor::CPU>(addr);
      }

      if (was_running) emulator_.run();
    }

    std::string base64_dump = Base64Encode(dump_data.data(), dump_data.size());

    return CreateSuccessResponse(req.value("id", json(nullptr)), {
      {"contents", {
        {
           {"uri", uri},
           {"mimeType", "application/octet-stream"},
           {"text", std::move(base64_dump)}
        }
      }}
    });
  }
  
  return CreateErrorResponse(req.value("id", json(nullptr)), -32602, "Resource not found");
}

std::string McpServer::HandleListPrompts(const std::string& json_req) {
  json req = json::parse(json_req);
  json prompts = {
    {
      {"name", "analyze_crash"},
      {"description", "Analyzes an Amiga crash (Guru Meditation) by injecting CPU state and memory context."},
      {"arguments", {
        {{"name", "context"}, {"description", "Optional additional context about what the user was doing", "required", false}}
      }}
    }
  };
  return CreateSuccessResponse(req.value("id", json(nullptr)), {{"prompts", prompts}});
}

std::string McpServer::HandleGetPrompt(const std::string& json_req) {
  json req;
  try {
    req = json::parse(json_req);
    std::string name = req.at("params").at("name").get<std::string>();

    if (name == "analyze_crash") {
        auto cpu_info = emulator_.cpu.getInfo();
        std::string prompt_text = std::format(
            "The Amiga crashed or needs analysis.\nPC: 0x{:08X}, SR: 0x{:04X}, USP: 0x{:08X}, ISP: 0x{:08X}\n"
            "Please analyze what might have caused the issue at this program counter.",
            cpu_info.pc0, cpu_info.sr, cpu_info.usp, cpu_info.isp);

        json result = {
          {"description", "Amiga crash state"},
          {"messages", json::array({
            {
              {"role", "user"},
              {"content", {
                {"type", "text"},
                {"text", prompt_text}
              }}
            }
          })}
        };
        return CreateSuccessResponse(req.value("id", json(nullptr)), result);
    }
  } catch (...) {
  }
  return CreateErrorResponse(req.value("id", json(nullptr)), -32602, "Prompt not found or invalid parameters");
}

}  // namespace gui
