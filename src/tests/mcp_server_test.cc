#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "services/mcp_server.h"
#include "VAmiga.h"

using json = nlohmann::json;

class McpServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<gui::McpServer>(*reinterpret_cast<vamiga::VAmiga*>(dummy_emulator_buf_));
  }

  void TearDown() override {
    server_->Stop();
  }

  alignas(vamiga::VAmiga) uint8_t dummy_emulator_buf_[sizeof(vamiga::VAmiga)] = {0};
  std::unique_ptr<gui::McpServer> server_;
};

TEST_F(McpServerTest, TestInitialize) {
  json req = {
    {"jsonrpc", "2.0"},
    {"id", 1},
    {"method", "initialize"}
  };
  
  std::string response = server_->ProcessMessage(req.dump());
  json res = json::parse(response);
  
  EXPECT_EQ(res["jsonrpc"], "2.0");
  EXPECT_EQ(res["id"], 1);
  EXPECT_TRUE(res.contains("result"));
  EXPECT_EQ(res["result"]["serverInfo"]["name"], "vAmigaImgui-MCP");
}

TEST_F(McpServerTest, TestListTools) {
  json req = {
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "tools/list"}
  };
  
  std::string response = server_->ProcessMessage(req.dump());
  json res = json::parse(response);
  
  EXPECT_TRUE(res.contains("result"));
  EXPECT_TRUE(res["result"].contains("tools"));
  EXPECT_GT(res["result"]["tools"].size(), 0);
}

TEST_F(McpServerTest, TestMethodNotFound) {
  json req = {
    {"jsonrpc", "2.0"},
    {"id", 3},
    {"method", "invalid_method"}
  };
  
  std::string response = server_->ProcessMessage(req.dump());
  json res = json::parse(response);
  
  EXPECT_TRUE(res.contains("error"));
  EXPECT_EQ(res["error"]["code"], -32601);
}

TEST_F(McpServerTest, TestPing) {
  json req = {
    {"jsonrpc", "2.0"},
    {"id", 4},
    {"method", "ping"}
  };
  
  std::string response = server_->ProcessMessage(req.dump());
  json res = json::parse(response);
  
  EXPECT_EQ(res["jsonrpc"], "2.0");
  EXPECT_TRUE(res.contains("result"));
}

TEST_F(McpServerTest, TestListResources) {
  json req = {
    {"jsonrpc", "2.0"},
    {"id", 5},
    {"method", "resources/list"}
  };
  
  std::string response = server_->ProcessMessage(req.dump());
  json res = json::parse(response);
  
  EXPECT_TRUE(res.contains("result"));
  EXPECT_TRUE(res["result"].contains("resources"));
  EXPECT_GT(res["result"]["resources"].size(), 0);
}

TEST_F(McpServerTest, TestListPrompts) {
  json req = {
    {"jsonrpc", "2.0"},
    {"id", 6},
    {"method", "prompts/list"}
  };
  
  std::string response = server_->ProcessMessage(req.dump());
  json res = json::parse(response);
  
  EXPECT_TRUE(res.contains("result"));
  EXPECT_TRUE(res["result"].contains("prompts"));
  EXPECT_EQ(res["result"]["prompts"][0]["name"], "analyze_crash");
}

TEST_F(McpServerTest, TestLifecycle) {
  json req_shutdown = {
    {"jsonrpc", "2.0"},
    {"id", 7},
    {"method", "shutdown"}
  };
  
  std::string response = server_->ProcessMessage(req_shutdown.dump());
  json res = json::parse(response);
  EXPECT_TRUE(res.contains("result"));

  json req_exit = {
    {"jsonrpc", "2.0"},
    {"method", "exit"}
  };
  std::string exit_res = server_->ProcessMessage(req_exit.dump());
  EXPECT_EQ(exit_res, "__EXIT__");
}

TEST_F(McpServerTest, TestReadResources) {
  {
    json req = {
      {"jsonrpc", "2.0"},
      {"id", 700},
      {"method", "resources/read"},
      {"params", {{"uri", "amiga://screen/current"}}}
    };
    json res = json::parse(server_->ProcessMessage(req.dump()));
    EXPECT_EQ(res["id"], 700);
    EXPECT_TRUE(res.contains("result"));
    auto contents = res["result"]["contents"];
    EXPECT_EQ(contents.size(), 1);
    EXPECT_EQ(contents[0]["uri"], "amiga://screen/current");
    EXPECT_EQ(contents[0]["mimeType"], "image/bmp");
    EXPECT_TRUE(contents[0]["text"].get<std::string>().find("Qk") == 0);
  }
  
  {
    json req = {
      {"jsonrpc", "2.0"},
      {"id", 701},
      {"method", "resources/read"},
      {"params", {{"uri", "amiga://memory/dump"}}}
    };
    json res = json::parse(server_->ProcessMessage(req.dump()));
    EXPECT_EQ(res["id"], 701);
    EXPECT_TRUE(res.contains("result"));
    auto contents = res["result"]["contents"];
    EXPECT_EQ(contents.size(), 1);
    EXPECT_EQ(contents[0]["uri"], "amiga://memory/dump");
    EXPECT_EQ(contents[0]["mimeType"], "application/octet-stream");
    EXPECT_FALSE(contents[0]["text"].get<std::string>().empty());
  }
}

TEST_F(McpServerTest, TestDisassembleTool) {
  json req = {
    {"jsonrpc", "2.0"},
    {"id", 800},
    {"method", "tools/call"},
    {"params", {
      {"name", "disassemble"},
      {"arguments", {
        {"address", 0x1000},
        {"count", 5}
      }}
    }}
  };
  json res = json::parse(server_->ProcessMessage(req.dump()));
  EXPECT_EQ(res["id"], 800);
  EXPECT_TRUE(res.contains("result"));
  EXPECT_FALSE(res["result"]["isError"]);
  EXPECT_TRUE(res["result"]["content"][0]["text"].get<std::string>().find("CPU not initialized") != std::string::npos);
  
  json req_err = {
    {"jsonrpc", "2.0"},
    {"id", 801},
    {"method", "tools/call"},
    {"params", {
      {"name", "disassemble"},
      {"arguments", {
        {"address", 0x1000},
        {"count", 15000}
      }}
    }}
  };
  json res_err = json::parse(server_->ProcessMessage(req_err.dump()));
  EXPECT_EQ(res_err["id"], 801);
  EXPECT_TRUE(res_err["result"]["isError"]);
}

TEST_F(McpServerTest, TestStressConcurrent) {
  constexpr int num_threads = 20;
  constexpr int requests_per_thread = 500;
  std::atomic<int> success_count{0};

  auto worker = [&]() {
    for (int i = 0; i < requests_per_thread; ++i) {
      json req = {
        {"jsonrpc", "2.0"},
        {"id", i},
        {"method", "ping"}
      };
      std::string response = server_->ProcessMessage(req.dump());
      try {
        json res = json::parse(response);
        if (res["jsonrpc"] == "2.0" && res.contains("result")) {
          success_count++;
        }
      } catch (...) {}
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(success_count.load(), num_threads * requests_per_thread);
}

