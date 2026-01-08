#include "components/video_window.h"
#include "imgui.h"
#include "resources/IconsFontAwesome6.h"
namespace gui {
bool VideoWindow::Draw(bool* p_open, uint32_t texture_id, int scale_mode) {
  ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (!ImGui::Begin(ICON_FA_TV " Amiga Screen", p_open, flags)) {
    ImGui::End();
    return false;
  }
  bool is_hovered = ImGui::IsWindowHovered();
  ImVec2 avail = ImGui::GetContentRegionAvail();
  ImVec2 pos = ImGui::GetCursorPos();
  float aspect_amiga = (float)vamiga::HPIXELS / (float)vamiga::VPIXELS;
  float w = avail.x;
  float h = avail.y;
  if (scale_mode == 1) {
  } else if (scale_mode == 2) {
      float scale = 1.0f;
      while ((vamiga::HPIXELS * (scale + 1) <= avail.x) && (vamiga::VPIXELS * (scale + 1) <= avail.y)) {
          scale += 1.0f;
      }
      w = vamiga::HPIXELS * scale;
      h = vamiga::VPIXELS * scale;
  } else {
      float aspect_window = avail.x / avail.y;
      if (aspect_window > aspect_amiga) {
          w = h * aspect_amiga;
      } else {
          h = w / aspect_amiga;
      }
  }
  float offset_x = (avail.x - w) * 0.5f;
  float offset_y = (avail.y - h) * 0.5f;
  if (w > 0 && h > 0) {
      ImGui::SetCursorPos(ImVec2(pos.x + offset_x, pos.y + offset_y));
      ImGui::Image((void*)(intptr_t)texture_id, ImVec2(w, h), ImVec2(0, 0), ImVec2(1, 1));
  }
  ImGui::End();
  return is_hovered;
}
}