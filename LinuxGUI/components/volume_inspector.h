#ifndef LINUXGUI_COMPONENTS_VOLUME_INSPECTOR_H_
#define LINUXGUI_COMPONENTS_VOLUME_INSPECTOR_H_

#include <memory>
#include <string>
#include <vector>
#include "VAmiga.h"
#undef unreachable
#include "FileSystems/MutableFileSystem.h"
#include "imgui.h"

namespace gui {

class VolumeInspector {
 public:
  static VolumeInspector& Instance();

  void Open(int drive_nr, bool is_hd, vamiga::VAmiga& emu, int partition = 0);
  void Draw(vamiga::VAmiga& emu);

 private:
  VolumeInspector() = default;

  void Refresh(vamiga::VAmiga& emu);
  void DrawInfo();
  void DrawUsage();
  void DrawBlockView();

  bool open_ = false;
  int drive_nr_ = 0;
  bool is_hd_ = false;
  int part_ = 0;

  std::unique_ptr<vamiga::MutableFileSystem> fs_;
  vamiga::FSInfo info_{};
  std::vector<uint8_t> usage_map_;
  std::array<int, static_cast<size_t>(vamiga::FSBlockType::DATA_FFS) + 1> type_counts_{};

  int selected_block_ = 0;
};

}  // namespace gui

#endif
