#ifndef LINUXGUI_SERVICES_MCP_SERVER_H_
#define LINUXGUI_SERVICES_MCP_SERVER_H_

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <utility>
#include <memory>
#include <unordered_set>
#include <nlohmann/json_fwd.hpp>

#include "VAmiga.h"

namespace gui {

class McpServer {
 public:
  explicit McpServer(vamiga::VAmiga& emulator);
  ~McpServer();

  void Start(const std::string& host, int port);
  void Stop();
  bool IsRunning() const { return running_; }
  
  std::string ProcessMessage(const std::string& message);

 private:
  void RunServer();
  void HandleClient(int client_socket);
  void RunNotifications();
  std::string HandleInitialize(const std::string& json_req);
  std::string HandleListTools(const std::string& json_req);
  std::string HandleCallTool(const std::string& json_req);
  
  std::string HandleShutdown(const std::string& json_req);
  std::string HandleExit(const std::string& json_req);
  std::string HandleCancelRequest(const std::string& json_req);
  std::string HandleListResources(const std::string& json_req);
  std::string HandleReadResource(const std::string& json_req);
  std::string HandleListPrompts(const std::string& json_req);
  std::string HandleGetPrompt(const std::string& json_req);
  std::string HandleInitializedNotification(const std::string& json_req);

  std::string CreateErrorResponse(const nlohmann::json& id_val, int code, const std::string& msg);
  std::string CreateSuccessResponse(const nlohmann::json& id_val, const nlohmann::json& result_val);
  std::string Base64Encode(const uint8_t* data, size_t length);

  vamiga::VAmiga& emulator_;
  std::string host_ = "127.0.0.1";
  int port_ = 0;
  std::atomic<bool> running_{false};
  int server_socket_ = -1;
  std::thread server_thread_;
  std::thread notification_thread_;

  std::mutex clients_mutex_;
  std::unordered_set<int> active_sockets_;
  std::atomic<int> active_clients_{0};
};

}  // namespace gui

#endif  // LINUXGUI_SERVICES_MCP_SERVER_H_
