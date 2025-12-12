#ifndef LINUXGUI_COMPONENTS_SETTINGS_WINDOW_H_
#define LINUXGUI_COMPONENTS_SETTINGS_WINDOW_H_

#include <filesystem>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include "imgui.h"
#include "Infrastructure/Option.h"
#include "resources/IconsFontAwesome6.h"
#include <utility>
#include "VAmigaTypes.h"
#undef unreachable
namespace vamiga {
class VAmiga;
}
namespace gui {
struct SettingsContext {
  std::string* kickstart_path;
  std::string* ext_rom_path;
  std::string* floppy_paths[4];
  std::string* hard_drive_paths[4];
  bool* pause_in_background;
  bool* retain_mouse_by_click;
  bool* retain_mouse_by_entering;
  bool* release_mouse_by_shaking;
  int* volume;
  int* scale_mode;
  bool* is_fullscreen;
  bool* video_as_background;
  bool* snapshot_auto_delete;
  int* screenshot_format;
  int* screenshot_source;
  std::function<void(const std::filesystem::path&)> on_load_kickstart;
  std::function<void()> on_eject_kickstart;
  std::function<void(const std::filesystem::path&)> on_load_ext_rom;
  std::function<void()> on_eject_ext_rom;
  std::function<void(int, const std::filesystem::path&)> on_insert_floppy;
  std::function<void(int)> on_eject_floppy;
  std::function<void(int, const std::filesystem::path&)> on_attach_hd;
  std::function<void(int)> on_detach_hd;
  std::function<void()> on_save_config;
  std::function<void()> on_toggle_fullscreen;
};
class SettingsWindow {
 public:
  static SettingsWindow& Instance() {
    static SettingsWindow instance;
    return instance;
  }
  void Draw(bool* p_open, vamiga::VAmiga& emulator, const SettingsContext& ctx);
 private:
  SettingsWindow() = default;
  void DrawSidebar();
  void DrawContent(vamiga::VAmiga& emulator, const SettingsContext& ctx);
  void DrawGeneral(vamiga::VAmiga& emulator, const SettingsContext& ctx);
  void DrawInputs(vamiga::VAmiga& emulator, const SettingsContext& ctx);
  void DrawCaptures(vamiga::VAmiga& emulator, const SettingsContext& ctx);
  void DrawROMs(vamiga::VAmiga& emulator, const SettingsContext& ctx);
  void DrawHardware(vamiga::VAmiga& emulator);
  void DrawPeripherals(vamiga::VAmiga& emulator, const SettingsContext& ctx);
  void DrawPerformance(vamiga::VAmiga& emulator);
  void DrawCompatibility(vamiga::VAmiga& emulator);
  void DrawVideo(vamiga::VAmiga& emulator);
  void DrawAudio(vamiga::VAmiga& emulator, const SettingsContext& ctx);
  template <vamiga::Opt opt>
  void DrawEnumCombo(std::string_view label, vamiga::VAmiga& emu);
  void DrawRomInfo(std::string_view label, const vamiga::RomTraits& traits,
                   bool present, std::string* path_buffer,
                   std::function<void()> on_load,
                   std::function<void()> on_eject);
  int current_tab_ = 0;
};
}

template <vamiga::Opt opt>
void gui::SettingsWindow::DrawEnumCombo(std::string_view label, vamiga::VAmiga& emu) {
  static const auto items = vamiga::OptionParser::pairs(opt);
  const auto current_val = emu.get(opt);

  const auto preview_it = std::ranges::find_if(
      items, [current_val](const auto& item) { return item.second == current_val; });
  std::string_view preview = (preview_it != items.end()) ? preview_it->first : "Unknown";

  if (ImGui::BeginCombo(label.data(), preview.data())) {
    for (const auto& [name, value] : items) {
      const bool is_selected = (current_val == value);
      if (ImGui::Selectable(name.c_str(), is_selected)) {
        emu.set(opt, value);
      }
      if (is_selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
}

#endif
