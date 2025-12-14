#ifndef LINUXGUI_COMPONENTS_INSPECTOR_H_
#define LINUXGUI_COMPONENTS_INSPECTOR_H_
#include <array>
#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "VAmiga.h"
#undef unreachable
#define unreachable std::unreachable()
#include "imgui.h"
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
  struct TabDescriptor {
    Tab tab;
    std::string_view label;
  };
  static constexpr auto kTabs =
      std::to_array<TabDescriptor>({TabDescriptor{Tab::kCPU, "CPU"},
                                    TabDescriptor{Tab::kMemory, "Memory"},
                                    TabDescriptor{Tab::kAgnus, "Agnus"},
                                    TabDescriptor{Tab::kDenise, "Denise"},
                                    TabDescriptor{Tab::kPaula, "Paula"},
                                    TabDescriptor{Tab::kCIA, "CIA"},
                                    TabDescriptor{Tab::kCopper, "Copper"},
                                    TabDescriptor{Tab::kBlitter, "Blitter"},
                                    TabDescriptor{Tab::kEvents, "Events"},
                                    TabDescriptor{Tab::kPorts, "Ports"},
                                    TabDescriptor{Tab::kBus, "Bus"}});
  static constexpr std::string_view TabLabel(Tab tab) {
    for (const auto& entry : kTabs) {
      if (entry.tab == tab) return entry.label;
    }
    return "Unknown";
  }
  static constexpr Tab TabFromIndex(std::size_t idx) {
    return idx < kTabs.size() ? kTabs[idx].tab : Tab::kNone;
  }
  template <std::invocable<const TabDescriptor&> F>
  static constexpr void ForEachTab(F&& fn) {
    for (const auto& entry : kTabs) {
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
  void DrawRegister(std::string_view label, uint32_t val, int width = 32);
  void DrawBit(std::string_view label, bool set);
  void DrawHexDump(vamiga::VAmiga& emu, uint32_t addr, int rows,
                   vamiga::Accessor accessor);
 public:
  template <std::integral T>
  void SetDasmAddress(T addr) {
    dasm_addr_ = static_cast<int>(addr);
    follow_pc_ = false;
  }
 private:
  void DrawCopperList(int list_idx, bool symbolic, int extra_rows,
                      const vamiga::CopperInfo& info, vamiga::VAmiga& emu);

  int dasm_addr_ = 0;
  bool follow_pc_ = true;
  uint32_t mem_addr_ = 0;
  int mem_rows_ = 16;
  char mem_search_buf_[16] = "";
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
