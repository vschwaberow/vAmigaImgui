#ifndef LINUXGUI_COMPONENTS_VIDEO_WINDOW_H_
#define LINUXGUI_COMPONENTS_VIDEO_WINDOW_H_

#include "imgui.h"
#include <utility>
#include "VAmigaTypes.h"
namespace gui {
class VideoWindow {
 public:
  static VideoWindow& Instance() {
    static VideoWindow instance;
    return instance;
  }
  bool Draw(bool* p_open, uint32_t texture_id, int scale_mode);
 private:
  VideoWindow() = default;
};
}

#endif
