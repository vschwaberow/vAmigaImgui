#ifndef LINUXGUI_COMPONENTS_INSPECTOR_H_
#define LINUXGUI_COMPONENTS_INSPECTOR_H_
#include <array>
#include <charconv>
#include <concepts>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "VAmiga.h"
#undef unreachable
#define unreachable std::unreachable()
#include "imgui.h"
#include "resources/IconsFontAwesome6.h"
namespace gui {
class Inspector {
 public:
  static Inspector& Instance();
  void DrawAll(bool* primary_toggle, vamiga::VAmiga& emu);
  void OpenWindow();
 private:
  Inspector();
  enum class Tab {
    kCPU,
    kMemory,
    kAgnus,
    kDenise,
    kPaula,
    kCIA,
    kCopper,
    kBlitter,
    kEvents,
    kPorts,
    kBus,
    kNone
  };
  using TabDrawFn = void (Inspector::*)(vamiga::VAmiga&);
  struct TabDescriptor {
    Tab tab;
    std::string_view label;
    TabDrawFn draw;
  };
  static const std::array<TabDescriptor, 11> kTabDescriptors;
  static std::string_view TabLabel(Tab tab);
  static Tab TabFromIndex(std::size_t idx);
  template <std::invocable<const TabDescriptor&> F>
  static void ForEachTab(F&& fn) {
    for (const auto& entry : kTabDescriptors) {
      std::invoke(fn, entry);
    }
  }
  struct WindowState {
    bool open = true;
    int id = 0;
    Tab active_tab = Tab::kCPU;
  };

  void DrawWindow(WindowState& state, vamiga::VAmiga& emu);
  void DrawToolbar(vamiga::VAmiga& emu, WindowState& state);
  void DrawCPU(vamiga::VAmiga& emu);
  void DrawTrace(vamiga::VAmiga& emu);
  void DrawBreakpoints(vamiga::VAmiga& emu);
  void DrawMemory(vamiga::VAmiga& emu);
  void DrawMemoryMap(const vamiga::MemInfo& info, vamiga::Accessor accessor);
  void DrawAgnus(vamiga::VAmiga& emu);
  void DrawDenise(vamiga::VAmiga& emu);
  void DrawPaula(vamiga::VAmiga& emu);
  void DrawCIA(vamiga::VAmiga& emu);
  void DrawCopper(vamiga::VAmiga& emu);
  void DrawCopperBreakpoints(vamiga::VAmiga& emu);
  void DrawBlitter(vamiga::VAmiga& emu);
  void DrawPorts(vamiga::VAmiga& emu);
  void DrawWatchpoints(vamiga::VAmiga& emu);
  void DrawEvents(vamiga::VAmiga& emu);
  void DrawSprite(const vamiga::SpriteInfo& info, int id);
  template <std::integral T>
  void DrawRegister(std::string_view label, T val, int width = 32) {
    ImGui::Text("%s:", label.data());
    ImGui::SameLine();
    if (hex_mode_) {
      switch (width) {
        case 8: ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%02X",
                                   static_cast<uint32_t>(val));
                break;
        case 16: ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%04X",
                                    static_cast<uint32_t>(val));
                break;
        default: ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%08X",
                                    static_cast<uint32_t>(val));
      }
    } else {
      ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%u",
                         static_cast<uint32_t>(val));
    }
  }
  template <std::convertible_to<bool> T>
  void DrawBit(std::string_view label, T set) {
    const bool on = static_cast<bool>(set);
    if (on) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
      ImGui::Text(ICON_FA_SQUARE_CHECK " %s", label.data());
      ImGui::PopStyleColor();
    } else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      ImGui::Text(ICON_FA_SQUARE " %s", label.data());
      ImGui::PopStyleColor();
    }
  }
  void DrawHexDump(vamiga::VAmiga& emu, uint32_t addr, int rows,
                   vamiga::Accessor accessor);
 public:
  template <std::integral T>
  void SetDasmAddress(T addr) {
    dasm_addr_ = static_cast<int>(addr);
    follow_pc_ = false;
  }
 private:
  void DrawBus(vamiga::VAmiga& emu);
  void DrawCopperList(int list_idx, bool symbolic, int extra_rows,
                      const vamiga::CopperInfo& info, vamiga::VAmiga& emu);
  template <std::unsigned_integral T, std::size_t N>
  static bool HexInput(const char* id, std::array<char, N>& buffer, T& out) {
    if (ImGui::InputText(
            id, buffer.data(), buffer.size(),
            ImGuiInputTextFlags_CharsHexadecimal |
                ImGuiInputTextFlags_EnterReturnsTrue)) {
      T parsed = 0;
      if (auto [_, ec] = std::from_chars(buffer.data(),
                                         buffer.data() + std::strlen(buffer.data()),
                                         parsed, 16);
          ec == std::errc()) {
        out = parsed;
        return true;
      }
    }
    return false;
  }

  int dasm_addr_ = 0;
  bool follow_pc_ = true;
  uint32_t mem_addr_ = 0;
  int mem_rows_ = 16;
  std::array<char, 16> mem_search_buf_{};
  bool hex_mode_ = true;
  int selected_cia_ = 0;
  bool copper_symbolic_[2] = {true, true};
  int copper_extra_rows_[2] = {0, 0};
  int mem_accessor_ = 0;  // 0=CPU,1=Agnus
  int mem_selected_bank_ = 0;
  std::vector<WindowState> windows_{{true, 1, Tab::kCPU}};
  int next_id_ = 2;
};
}
#endif
