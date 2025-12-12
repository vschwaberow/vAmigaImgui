#ifndef LINUXGUI_COMPONENTS_VIRTUAL_KEYBOARD_H_
#define LINUXGUI_COMPONENTS_VIRTUAL_KEYBOARD_H_
#include <string_view>
#include <utility>
#include "VAmiga.h"
#undef unreachable
#include "imgui.h"
namespace gui {
class VirtualKeyboard {
 public:
  static VirtualKeyboard& Instance();
  void Draw(bool* p_open, vamiga::VAmiga& emu);
 private:
  VirtualKeyboard() = default;
  void DrawKey(vamiga::VAmiga& emu, std::string_view label, int width, int code);
};
}
#endif
