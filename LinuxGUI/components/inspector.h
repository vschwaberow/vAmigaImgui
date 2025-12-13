#ifndef LINUXGUI_COMPONENTS_INSPECTOR_H_
#define LINUXGUI_COMPONENTS_INSPECTOR_H_
#include <string>
#include <vector>
#include <utility>
#include "VAmiga.h"
#undef unreachable
#include "imgui.h"
namespace gui {
class Inspector {
 public:
  static Inspector& Instance();
  void Draw(bool* p_open, vamiga::VAmiga& emu);
 private:
  Inspector();
  void DrawToolbar(vamiga::VAmiga& emu);
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
  void SetDasmAddress(int addr) { dasm_addr_ = addr; follow_pc_ = false; }
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
};
}
#endif
