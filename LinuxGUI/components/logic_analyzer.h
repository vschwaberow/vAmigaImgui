#ifndef LINUXGUI_COMPONENTS_LOGIC_ANALYZER_H_
#define LINUXGUI_COMPONENTS_LOGIC_ANALYZER_H_
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include "VAmiga.h"
#undef unreachable
#include "imgui.h"
namespace gui {
class LogicAnalyzer {
 public:
  static LogicAnalyzer& Instance();
  void Draw(vamiga::VAmiga& emu);
  void Update(vamiga::VAmiga& emu);
 private:
  LogicAnalyzer();
  void DrawControls(vamiga::VAmiga& emu);
  void DrawBus(vamiga::VAmiga& emu, ImVec2 pos, float width, float height);
  void DrawSignal(vamiga::VAmiga& emu, int channel, ImVec2 pos, float width, float height);
  void DrawLabels(vamiga::VAmiga& emu, ImVec2 pos, float width, float height);
  void UpdateProbe(vamiga::VAmiga& emu, int channel, int probe_type, uint32_t addr);
  struct ProbePreset {
    std::string_view name;
    int type;
    uint32_t addr;
  };
  static const std::vector<ProbePreset> kPresets;
  static constexpr int kSegments = 228;
  static constexpr int kSignals = 6;
  float zoom_ = 1.0f;
  bool visible_ = true;
  bool hex_mode_ = true;
  bool symbolic_ = false;
  std::array<std::array<int, kSegments>, kSignals> data_;
  std::array<std::string, kSegments> labels_;
  std::array<ImU32, kSegments> colors_;
  int probe_types_[4] = {0, 0, 0, 0};
  uint32_t probe_addrs_[4] = {0, 0, 0, 0};
};
}
#endif
