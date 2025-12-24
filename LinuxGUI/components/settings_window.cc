#include "components/settings_window.h"
#include "input_manager.h"
#include <array>
#include <exception>
#include <format>
#include <ranges>
#include <span>
#include "VAmiga.h"
#include "constants.h"
#include "Infrastructure/Option.h"
#include "components/file_picker.h"
#include "imgui.h"
namespace ImGui {
inline bool InputText(const char* label, std::string* str,
               ImGuiInputTextFlags flags = 0) {
  return InputText(
      label, str->data(), str->capacity() + 1,
      flags | ImGuiInputTextFlags_CallbackResize,
      [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
          std::string* s = (std::string*)data->UserData;
          s->resize(data->BufTextLen);
          data->Buf = s->data();
        }
        return 0;
      },
      str);
}
}

namespace {
template <std::size_t N>
bool DrawValueCombo(const char* label, int& current_value,
                    const std::array<const char*, N>& labels,
                    const std::array<int, N>& values) {
  const auto it = std::ranges::find(values, current_value);
  int selected = (it != values.end())
                     ? static_cast<int>(std::ranges::distance(values.begin(), it))
                     : 0;

  if (ImGui::Combo(label, &selected, labels.data(),
                   static_cast<int>(labels.size()))) {
    current_value = values[selected];
    return true;
  }
  return false;
}

struct MediaSlotDescriptor {
  std::string_view icon;
  std::string_view prefix;
  std::string_view title_prefix;
  std::string_view popup_prefix;
  std::string_view attach_label;
  std::string_view detach_label;
  std::string_view filters;
  ImVec2 button_size{60, 0};
  int id_offset = 0;
};

template <typename PathsSpan, typename AttachFn, typename DetachFn>
void DrawMediaSlots(const MediaSlotDescriptor& desc, PathsSpan paths,
                    AttachFn on_attach, DetachFn on_detach) {
  for (int i : std::views::iota(0, static_cast<int>(paths.size()))) {
    ImGui::PushID(desc.id_offset + i);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s %s%d:", desc.icon.data(), desc.prefix.data(), i);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-160);
    ImGui::InputText("##path", paths[i]);
    ImGui::SameLine();
    if (ImGui::Button(desc.attach_label.data(), desc.button_size)) {
      gui::PickerOptions opts;
      opts.title = std::format("{} {}{}", desc.title_prefix, desc.prefix, i);
      opts.filters = desc.filters;
      gui::FilePicker::Instance().Open(
          std::format("{}{}", desc.popup_prefix, i), opts,
          [on_attach, i](std::filesystem::path p) { on_attach(i, std::move(p)); });
    }
    ImGui::SameLine();
    if (ImGui::Button(desc.detach_label.data(), desc.button_size)) {
      on_detach(i);
    }
    ImGui::PopID();
  }
}
}  // namespace
namespace gui {
void SettingsWindow::Draw(bool* p_open, vamiga::VAmiga& emulator,
                          const SettingsContext& ctx) {
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(ICON_FA_GEARS " Settings", p_open)) {
    ImGui::End();
    return;
  }
  if (ImGui::BeginTable("SettingsTable", 2,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawSidebar();
    ImGui::TableSetColumnIndex(1);
    DrawContent(emulator, ctx);
    ImGui::EndTable();
  }
  if (!error_message_.empty()) ImGui::OpenPopup("Settings Error");
  if (ImGui::BeginPopupModal("Settings Error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", error_message_.c_str());
    ImGui::Separator();
    if (ImGui::Button("Close")) {
      error_message_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}
void SettingsWindow::DrawSidebar() {
  ImGui::Spacing();
  ImGui::TextDisabled("EMULATOR");
  if (ImGui::Selectable(ICON_FA_GEAR " General", current_tab_ == 0)) current_tab_ = 0;
  if (ImGui::Selectable(ICON_FA_CAMERA " Captures", current_tab_ == 1)) current_tab_ = 1;
  if (ImGui::Selectable(ICON_FA_KEYBOARD " Inputs", current_tab_ == 2)) current_tab_ = 2;
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::TextDisabled("VIRTUAL MACHINE");
  if (ImGui::Selectable(ICON_FA_MICROCHIP " ROMs", current_tab_ == 3)) current_tab_ = 3;
  if (ImGui::Selectable(ICON_FA_MEMORY " Hardware", current_tab_ == 4)) current_tab_ = 4;
  if (ImGui::Selectable(ICON_FA_FLOPPY_DISK " Peripherals", current_tab_ == 5)) current_tab_ = 5;
  if (ImGui::Selectable(ICON_FA_GAUGE_HIGH " Performance", current_tab_ == 6)) current_tab_ = 6;
  if (ImGui::Selectable(ICON_FA_PUZZLE_PIECE " Compatibility", current_tab_ == 7)) current_tab_ = 7;
  if (ImGui::Selectable(ICON_FA_VOLUME_HIGH " Audio", current_tab_ == 8)) current_tab_ = 8;
  if (ImGui::Selectable(ICON_FA_EYE " Video", current_tab_ == 9)) current_tab_ = 9;
}
void SettingsWindow::DrawContent(vamiga::VAmiga& emulator, const SettingsContext& ctx) {
  ImGui::BeginChild("ContentRegion", ImVec2(0, 0), false, 0);
  switch (current_tab_) {
    case 0: DrawGeneral(emulator, ctx); break;
    case 1: DrawCaptures(emulator, ctx); break;
    case 2: DrawInputs(emulator, ctx); break;
    case 3: DrawROMs(emulator, ctx); break;
    case 4: DrawHardware(emulator); break;
    case 5: DrawPeripherals(emulator, ctx); break;
    case 6: DrawPerformance(emulator); break;
    case 7: DrawCompatibility(emulator); break;
    case 8: DrawAudio(emulator, ctx); break;
    case 9: DrawVideo(emulator); break;
  }
  ImGui::EndChild();
}
void SettingsWindow::DrawGeneral(vamiga::VAmiga& emulator, const SettingsContext& ctx) {
  ImGui::Text("Display & Layout");
  ImGui::Separator();
  if (ctx.scale_mode) {
      static constexpr std::array scale_items = { "Fit (Keep Aspect)", "Stretch", "Integer Scale" };
      ImGui::Combo("Scale Mode", ctx.scale_mode, scale_items.data(), scale_items.size());
  }
  if (ctx.video_as_background) {
      if (ImGui::Checkbox("Render Video as Background", ctx.video_as_background)) {
      }
      if (*ctx.video_as_background) {
          ImGui::TextDisabled("Video fills the main window behind UI.");
      } else {
          ImGui::TextDisabled("Video rendered in a dockable window.");
      }
  }
  if (ctx.is_fullscreen && ctx.on_toggle_fullscreen) {
      bool fs = *ctx.is_fullscreen;
      if (ImGui::Checkbox("Fullscreen", &fs)) {
          ctx.on_toggle_fullscreen();
      }
      ImGui::SameLine();
      ImGui::TextDisabled("(Alt+Enter)");
  }
  ImGui::Separator();
  ImGui::Text("Application");
  if (ctx.pause_in_background) {
      ImGui::Checkbox("Pause in background", ctx.pause_in_background);
  }
}

template <vamiga::Opt opt>
void SettingsWindow::DrawEnumCombo(std::string_view label, vamiga::VAmiga& emu) {
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
void SettingsWindow::DrawInputs(vamiga::VAmiga& emulator, const SettingsContext& ctx) {
  ImGui::Text("Input Ports");
  ImGui::Separator();
  if (ctx.port1_device && ctx.port2_device) {
    static const int kMaxDevices = 8;
    if (ImGui::BeginCombo("Port 1", InputManager::GetDeviceName(*ctx.port1_device).data())) {
      for (int i = 0; i < kMaxDevices; ++i) {
        bool is_selected = (*ctx.port1_device == i);
        if (ImGui::Selectable(InputManager::GetDeviceName(i).data(), is_selected)) {
          *ctx.port1_device = i;
          if (ctx.on_port_changed) ctx.on_port_changed();
          if (ctx.on_save_config) ctx.on_save_config();
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Port 2", InputManager::GetDeviceName(*ctx.port2_device).data())) {
      for (int i = 0; i < kMaxDevices; ++i) {
        bool is_selected = (*ctx.port2_device == i);
        if (ImGui::Selectable(InputManager::GetDeviceName(i).data(), is_selected)) {
          *ctx.port2_device = i;
          if (ctx.on_port_changed) ctx.on_port_changed();
          if (ctx.on_save_config) ctx.on_save_config();
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
  }
  ImGui::Separator();
  ImGui::Text("Autofire");
  bool af = static_cast<bool>(emulator.get(vamiga::Opt::JOY_AUTOFIRE));
  if (ImGui::Checkbox("Enable Autofire", &af)) {
      emulator.set(vamiga::Opt::JOY_AUTOFIRE, af);
  }
  
  ImGui::BeginDisabled(!af);
  bool bursts = static_cast<bool>(emulator.get(vamiga::Opt::JOY_AUTOFIRE_BURSTS));
  if (ImGui::Checkbox("Burst Mode", &bursts)) {
      emulator.set(vamiga::Opt::JOY_AUTOFIRE_BURSTS, bursts);
  }
  
  int bullets = static_cast<int>(emulator.get(vamiga::Opt::JOY_AUTOFIRE_BULLETS));
  if (ImGui::SliderInt("Bullets per Burst", &bullets, 1, 10)) {
      emulator.set(vamiga::Opt::JOY_AUTOFIRE_BULLETS, bullets);
  }
  
  int delay = static_cast<int>(emulator.get(vamiga::Opt::JOY_AUTOFIRE_DELAY));
  if (ImGui::SliderInt("Autofire Delay (Frames)", &delay, 1, 50)) {
      emulator.set(vamiga::Opt::JOY_AUTOFIRE_DELAY, delay);
  }
  ImGui::EndDisabled();

  ImGui::Spacing();
  ImGui::Text("Input Settings");
  ImGui::Separator();
  bool changed = false;
  ImGui::Text("Mouse Capture");
  if (ImGui::Checkbox("Retain mouse by clicking in window", ctx.retain_mouse_by_click)) changed = true;
  if (ImGui::Checkbox("Retain mouse by entering window", ctx.retain_mouse_by_entering)) changed = true;
  ImGui::Separator();
  ImGui::Text("Mouse Release");
  if (ImGui::Checkbox("Release mouse by shaking", ctx.release_mouse_by_shaking)) changed = true;
  ImGui::TextDisabled("Note: You can always release the mouse by pressing Ctrl+G");
  if (changed && ctx.on_save_config) ctx.on_save_config();
}
void SettingsWindow::DrawROMs(vamiga::VAmiga& emulator, const SettingsContext& ctx) {
  ImGui::Text("ROM Configuration");
  ImGui::Separator();
  ImGui::Spacing();
  auto rt = emulator.mem.getRomTraits();
  DrawRomInfo("KICKSTART ROM", rt, rt.crc != 0, ctx.kickstart_path,
              ctx.on_load_kickstart ? [ctx](){ ctx.on_load_kickstart(*ctx.kickstart_path); } : std::function<void()>(),
              ctx.on_eject_kickstart);
  ImGui::Separator();
  ImGui::Spacing();
  auto et = emulator.mem.getExtTraits();
  DrawRomInfo("EXTENDED ROM", et, et.crc != 0, ctx.ext_rom_path,
              ctx.on_load_ext_rom ? [ctx](){ ctx.on_load_ext_rom(*ctx.ext_rom_path); } : std::function<void()>(),
              ctx.on_eject_ext_rom);
}
void SettingsWindow::DrawHardware(vamiga::VAmiga& emulator) {
  auto try_set = [this, &emulator](vamiga::Opt opt, auto value) {
    try {
      emulator.set(opt, value);
    } catch (const std::exception& e) {
      error_message_ = e.what();
    }
  };
  ImGui::Text("CPU & Chipset");
  ImGui::Separator();
  int cpu = static_cast<int>(emulator.get(vamiga::Opt::CPU_REVISION));
  static constexpr std::array cpu_items = { "68000", "68010", "68EC020" };
  if (ImGui::Combo("CPU Model", &cpu, cpu_items.data(), cpu_items.size())) {
    try_set(vamiga::Opt::CPU_REVISION, cpu);
  }
  int agnus = static_cast<int>(emulator.get(vamiga::Opt::AGNUS_REVISION));
  static constexpr std::array agnus_items = { "OCS (A1000)", "OCS (A500/A2000)", "ECS (1MB)", "ECS (2MB)" };
  if (ImGui::Combo("Agnus", &agnus, agnus_items.data(), agnus_items.size())) {
    try_set(vamiga::Opt::AGNUS_REVISION, agnus);
  }
  int denise = static_cast<int>(emulator.get(vamiga::Opt::DENISE_REVISION));
  static constexpr std::array denise_items = { "OCS", "ECS" };
  if (ImGui::Combo("Denise", &denise, denise_items.data(), denise_items.size())) {
    try_set(vamiga::Opt::DENISE_REVISION, denise);
  }
  ImGui::Spacing();
  ImGui::Text("Memory");
  ImGui::Separator();
  bool mem_locked = emulator.isPoweredOn();
  if (mem_locked) {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                       ICON_FA_TRIANGLE_EXCLAMATION " Power off to change memory size.");
  }
  ImGui::BeginDisabled(mem_locked);
  int chip_ram = static_cast<int>(emulator.get(vamiga::Opt::MEM_CHIP_RAM));
  static constexpr auto chip_items = std::to_array<const char*>({"512 KB", "1 MB", "2 MB"});
  static constexpr auto chip_values = std::to_array<int>({512, 1024, 2048});
  if (DrawValueCombo("Chip RAM", chip_ram, chip_items, chip_values)) {
    try_set(vamiga::Opt::MEM_CHIP_RAM, chip_ram);
  }
  int slow_ram = static_cast<int>(emulator.get(vamiga::Opt::MEM_SLOW_RAM));
  static constexpr auto slow_items = std::to_array<const char*>({"None", "512 KB", "1 MB", "1.5 MB"});
  static constexpr auto slow_values = std::to_array<int>({0, 512, 1024, 1536});
  if (DrawValueCombo("Slow RAM", slow_ram, slow_items, slow_values)) {
    try_set(vamiga::Opt::MEM_SLOW_RAM, slow_ram);
  }
  int fast_ram = static_cast<int>(emulator.get(vamiga::Opt::MEM_FAST_RAM));
  static constexpr auto fast_items = std::to_array<const char*>({"None", "512 KB", "1 MB", "2 MB", "4 MB", "8 MB"});
  static constexpr auto fast_values = std::to_array<int>({0, 512, 1024, 2048, 4096, 8192});
  if (DrawValueCombo("Fast RAM", fast_ram, fast_items, fast_values)) {
    try_set(vamiga::Opt::MEM_FAST_RAM, fast_ram);
  }
  ImGui::EndDisabled();
  ImGui::Spacing();
  if (ImGui::Button("Hard Reset Machine")) {
      emulator.hardReset();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(Required for some changes to take effect)");
}

void SettingsWindow::DrawPerformance(vamiga::VAmiga& emulator) {
  ImGui::Text("Warp Mode");
  ImGui::Separator();
  DrawEnumCombo<vamiga::Opt::AMIGA_WARP_MODE>("Activation", emulator);
  int warp_boot = static_cast<int>(emulator.get(vamiga::Opt::AMIGA_WARP_BOOT));
  if (ImGui::InputInt("Warp Boot (sec)", &warp_boot)) {
      emulator.set(vamiga::Opt::AMIGA_WARP_BOOT, warp_boot);
  }

  ImGui::Spacing();
  ImGui::Text("Threading");
  ImGui::Separator();
  bool vsync = static_cast<bool>(emulator.get(vamiga::Opt::AMIGA_VSYNC));
  if (ImGui::Checkbox("VSync", &vsync)) {
      emulator.set(vamiga::Opt::AMIGA_VSYNC, vsync);
  }
  
  if (!vsync) {
      int speed = static_cast<int>(emulator.get(vamiga::Opt::AMIGA_SPEED_BOOST));
      if (ImGui::SliderInt("Speed Boost", &speed, 0, 1000, "%d%%")) {
          emulator.set(vamiga::Opt::AMIGA_SPEED_BOOST, speed);
      }
  } else {
      ImGui::TextDisabled("Speed Boost disabled (VSync active)");
  }
  
  int runahead = static_cast<int>(emulator.get(vamiga::Opt::AMIGA_RUN_AHEAD));
  if (ImGui::SliderInt("Run Ahead", &runahead, -5, 5)) {
      emulator.set(vamiga::Opt::AMIGA_RUN_AHEAD, runahead);
  }

  ImGui::Spacing();
  ImGui::Text("Boosters");
  ImGui::Separator();
  bool cia = static_cast<bool>(emulator.get(vamiga::Opt::CIA_IDLE_SLEEP));
  if (ImGui::Checkbox("CIA Idle Sleep", &cia)) {
      emulator.set(vamiga::Opt::CIA_IDLE_SLEEP, cia);
  }
  bool frame_skip = static_cast<bool>(emulator.get(vamiga::Opt::DENISE_FRAME_SKIPPING));
  if (ImGui::Checkbox("Frame Skipping (Warp)", &frame_skip)) {
      emulator.set(vamiga::Opt::DENISE_FRAME_SKIPPING, frame_skip);
  }
  bool aud_fast = static_cast<bool>(emulator.get(vamiga::Opt::AUD_FASTPATH));
  if (ImGui::Checkbox("Audio Fast Path", &aud_fast)) {
      emulator.set(vamiga::Opt::AUD_FASTPATH, aud_fast);
  }
}

void SettingsWindow::DrawCompatibility(vamiga::VAmiga& emulator) {
  ImGui::Text("Blitter");
  ImGui::Separator();
  int blit = static_cast<int>(emulator.get(vamiga::Opt::BLITTER_ACCURACY));
  if (ImGui::SliderInt("Accuracy", &blit, 0, 2)) {
      emulator.set(vamiga::Opt::BLITTER_ACCURACY, blit);
  }
  
  ImGui::Spacing();
  ImGui::Text("Chipset");
  ImGui::Separator();
  bool tod = static_cast<bool>(emulator.get(vamiga::Opt::CIA_TODBUG));
  if (ImGui::Checkbox("TOD Bug", &tod)) {
      emulator.set(vamiga::Opt::CIA_TODBUG, tod);
  }
  bool ptr = static_cast<bool>(emulator.get(vamiga::Opt::AGNUS_PTR_DROPS));
  if (ImGui::Checkbox("Pointer Drops", &ptr)) {
      emulator.set(vamiga::Opt::AGNUS_PTR_DROPS, ptr);
  }
  
  ImGui::Spacing();
  ImGui::Text("Disk Controller");
  ImGui::Separator();
  int dc_speed = static_cast<int>(emulator.get(vamiga::Opt::DC_SPEED));
  if (ImGui::InputInt("Drive Speed", &dc_speed)) {
      emulator.set(vamiga::Opt::DC_SPEED, dc_speed);
  }
  
  DrawEnumCombo<vamiga::Opt::DRIVE_MECHANICS>("Mechanics", emulator);
  
  bool lock_sync = static_cast<bool>(emulator.get(vamiga::Opt::DC_LOCK_DSKSYNC));
  if (ImGui::Checkbox("Lock DskSync", &lock_sync)) {
      emulator.set(vamiga::Opt::DC_LOCK_DSKSYNC, lock_sync);
  }
  bool auto_sync = static_cast<bool>(emulator.get(vamiga::Opt::DC_AUTO_DSKSYNC));
  if (ImGui::Checkbox("Auto DskSync", &auto_sync)) {
      emulator.set(vamiga::Opt::DC_AUTO_DSKSYNC, auto_sync);
  }
  
  ImGui::Spacing();
  ImGui::Text("Timing");
  ImGui::Separator();
  bool eclock = static_cast<bool>(emulator.get(vamiga::Opt::CIA_ECLOCK_SYNCING));
  if (ImGui::Checkbox("E-Clock Syncing", &eclock)) {
      emulator.set(vamiga::Opt::CIA_ECLOCK_SYNCING, eclock);
  }
  
  ImGui::Spacing();
  ImGui::Text("Keyboard");
  ImGui::Separator();
  bool kbd = static_cast<bool>(emulator.get(vamiga::Opt::KBD_ACCURACY));
  if (ImGui::Checkbox("Accurate Timing", &kbd)) {
      emulator.set(vamiga::Opt::KBD_ACCURACY, kbd);
  }
  
  ImGui::Spacing();
  ImGui::Text("Collisions");
  ImGui::Separator();
  bool spr_spr = static_cast<bool>(emulator.get(vamiga::Opt::DENISE_CLX_SPR_SPR));
  if (ImGui::Checkbox("Sprite-Sprite", &spr_spr)) {
      emulator.set(vamiga::Opt::DENISE_CLX_SPR_SPR, spr_spr);
  }
  bool spr_plf = static_cast<bool>(emulator.get(vamiga::Opt::DENISE_CLX_SPR_PLF));
  if (ImGui::Checkbox("Sprite-Playfield", &spr_plf)) {
      emulator.set(vamiga::Opt::DENISE_CLX_SPR_PLF, spr_plf);
  }
  bool plf_plf = static_cast<bool>(emulator.get(vamiga::Opt::DENISE_CLX_PLF_PLF));
  if (ImGui::Checkbox("Playfield-Playfield", &plf_plf)) {
      emulator.set(vamiga::Opt::DENISE_CLX_PLF_PLF, plf_plf);
  }
}

void SettingsWindow::DrawCaptures(vamiga::VAmiga& emulator, const SettingsContext& ctx) {
    ImGui::Text("Snapshots");
    ImGui::Separator();
    if (ctx.snapshot_auto_delete) {
        bool auto_del = *ctx.snapshot_auto_delete;
        if (ImGui::Checkbox("Auto-Delete Oldest Snapshot", &auto_del)) {
            *ctx.snapshot_auto_delete = auto_del;
            if (ctx.on_save_config) ctx.on_save_config();
        }
        ImGui::TextDisabled("Limits snapshots to %d.", gui::Defaults::kSnapshotLimit);
    }
    
    ImGui::Spacing();
    ImGui::Text("Screenshots");
    ImGui::Separator();
    
    if (ctx.screenshot_format) {
        static constexpr std::array formats = { "BMP" };
        ImGui::Combo("Format", ctx.screenshot_format, formats.data(), formats.size());
        ImGui::TextDisabled("Only BMP is supported currently.");
    }
    
    if (ctx.screenshot_source) {
        static constexpr std::array sources = { "Framebuffer" };
        ImGui::Combo("Source", ctx.screenshot_source, sources.data(), sources.size());
    }
}

void SettingsWindow::DrawPeripherals(vamiga::VAmiga& emulator, const SettingsContext& ctx) {
  ImGui::Text("Peripherals");
  ImGui::Separator();
  if (ImGui::BeginTabBar("StorageTabs")) {
    if (ImGui::BeginTabItem("Floppy Drives")) {
      ImGui::Spacing();
      static constexpr MediaSlotDescriptor floppy_desc{
          ICON_FA_FLOPPY_DISK, "DF", "Select Floppy", "Floppy", "Insert",
          "Eject",
          "Disk Files (*.adf *.adz *.dms *.ipf){.adf,.adz,.dms,.ipf},All Files (*.*){.*}"};

      DrawMediaSlots(floppy_desc,
                     std::span(ctx.floppy_paths, gui::kFloppyDriveCount),
                     [ctx](int i, std::filesystem::path p) {
                       ctx.on_insert_floppy(i, std::move(p));
                     },
                     [ctx](int i) { ctx.on_eject_floppy(i); });
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Hard Drives")) {
        ImGui::Spacing();
        static constexpr MediaSlotDescriptor hd_desc{
            ICON_FA_HARD_DRIVE, "HD", "Select Hard Drive", "HardDrive",
            "Attach", "Detach",
            "Hard Files (*.hdf){.hdf},All Files (*.*){.*}", ImVec2(60, 0), 100};

        DrawMediaSlots(hd_desc,
                       std::span(ctx.hard_drive_paths, gui::kHardDriveCount),
                       [ctx](int i, std::filesystem::path p) {
                         ctx.on_attach_hd(i, std::move(p));
                       },
                       [ctx](int i) { ctx.on_detach_hd(i); });
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}


void SettingsWindow::DrawVideo(vamiga::VAmiga& emulator) {
  ImGui::Text("Color");
  ImGui::Separator();
  DrawEnumCombo<vamiga::Opt::MON_PALETTE>("Palette", emulator);
  
  int brightness = static_cast<int>(emulator.get(vamiga::Opt::MON_BRIGHTNESS));
  if (ImGui::SliderInt("Brightness", &brightness, -100, 100, "%d%%")) {
      emulator.set(vamiga::Opt::MON_BRIGHTNESS, brightness);
  }
  int contrast = static_cast<int>(emulator.get(vamiga::Opt::MON_CONTRAST));
  if (ImGui::SliderInt("Contrast", &contrast, -100, 100, "%d%%")) {
      emulator.set(vamiga::Opt::MON_CONTRAST, contrast);
  }
  int saturation = static_cast<int>(emulator.get(vamiga::Opt::MON_SATURATION));
  if (ImGui::SliderInt("Saturation", &saturation, -100, 100, "%d%%")) {
      emulator.set(vamiga::Opt::MON_SATURATION, saturation);
  }

  ImGui::Spacing();
  ImGui::Text("Geometry");
  ImGui::Separator();
  DrawEnumCombo<vamiga::Opt::MON_ZOOM>("Zoom Mode", emulator);
  DrawEnumCombo<vamiga::Opt::MON_CENTER>("Centering", emulator);
  
  ImGui::Spacing();
  ImGui::Text("CRT Effects");
  ImGui::Separator();
  
  DrawEnumCombo<vamiga::Opt::MON_SCANLINES>("Scanlines", emulator);
  int scanlines = static_cast<int>(emulator.get(vamiga::Opt::MON_SCANLINES));
  if (scanlines > 0) {
      int scan_weight = static_cast<int>(emulator.get(vamiga::Opt::MON_SCANLINE_WEIGHT));
      if (ImGui::SliderInt("Scanline Weight", &scan_weight, 0, 100)) {
          emulator.set(vamiga::Opt::MON_SCANLINE_WEIGHT, scan_weight);
      }
  }
  
  DrawEnumCombo<vamiga::Opt::MON_DOTMASK>("Dotmask", emulator);
  
  bool blur = static_cast<bool>(emulator.get(vamiga::Opt::MON_BLUR));
  if (ImGui::Checkbox("Blur", &blur)) {
      emulator.set(vamiga::Opt::MON_BLUR, blur);
  }
  if (blur) {
      int blur_radius = static_cast<int>(emulator.get(vamiga::Opt::MON_BLUR_RADIUS));
      if (ImGui::SliderInt("Blur Radius", &blur_radius, 0, 10)) {
          emulator.set(vamiga::Opt::MON_BLUR_RADIUS, blur_radius);
      }
  }
  
  bool bloom = static_cast<bool>(emulator.get(vamiga::Opt::MON_BLOOM));
  if (ImGui::Checkbox("Bloom", &bloom)) {
      emulator.set(vamiga::Opt::MON_BLOOM, bloom);
  }
  if (bloom) {
      int bloom_weight = static_cast<int>(emulator.get(vamiga::Opt::MON_BLOOM_WEIGHT));
      if (ImGui::SliderInt("Bloom Weight", &bloom_weight, 0, 100)) {
          emulator.set(vamiga::Opt::MON_BLOOM_WEIGHT, bloom_weight);
      }
  }
  
  DrawEnumCombo<vamiga::Opt::MON_ENHANCER>("Enhancer", emulator);
  DrawEnumCombo<vamiga::Opt::MON_UPSCALER>("Upscaler", emulator);
  
  bool flicker = static_cast<bool>(emulator.get(vamiga::Opt::MON_FLICKER));
  if (ImGui::Checkbox("Interlace Flicker", &flicker)) {
      emulator.set(vamiga::Opt::MON_FLICKER, flicker);
  }
  if (flicker) {
      int flicker_weight = static_cast<int>(emulator.get(vamiga::Opt::MON_FLICKER_WEIGHT));
      if (ImGui::SliderInt("Flicker Weight", &flicker_weight, 0, 100)) {
          emulator.set(vamiga::Opt::MON_FLICKER_WEIGHT, flicker_weight);
      }
  }
  
  bool disalign = static_cast<bool>(emulator.get(vamiga::Opt::MON_DISALIGNMENT));
  if (ImGui::Checkbox("Color Disalignment", &disalign)) {
      emulator.set(vamiga::Opt::MON_DISALIGNMENT, disalign);
  }
  if (disalign) {
      int dh = static_cast<int>(emulator.get(vamiga::Opt::MON_DISALIGNMENT_H));
      if (ImGui::SliderInt("Horizontal Shift", &dh, -10, 10)) {
          emulator.set(vamiga::Opt::MON_DISALIGNMENT_H, dh);
      }
      int dv = static_cast<int>(emulator.get(vamiga::Opt::MON_DISALIGNMENT_V));
      if (ImGui::SliderInt("Vertical Shift", &dv, -10, 10)) {
          emulator.set(vamiga::Opt::MON_DISALIGNMENT_V, dv);
      }
  }
}

// Explicit instantiations for the enum combos we use.
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::AMIGA_WARP_MODE>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::DRIVE_MECHANICS>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::MON_PALETTE>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::MON_ZOOM>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::MON_CENTER>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::MON_SCANLINES>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::MON_DOTMASK>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::MON_ENHANCER>(
    std::string_view, vamiga::VAmiga&);
template void SettingsWindow::DrawEnumCombo<vamiga::Opt::MON_UPSCALER>(
    std::string_view, vamiga::VAmiga&);

void SettingsWindow::DrawAudio(vamiga::VAmiga& emulator, const SettingsContext& ctx) {
  ImGui::Text("Audio Settings");
  ImGui::Separator();
  if (ImGui::SliderInt("Master Volume", ctx.volume, 0, 100, "%d%%")) {
    emulator.set(vamiga::Opt::AUD_VOLL, *ctx.volume);
    emulator.set(vamiga::Opt::AUD_VOLR, *ctx.volume);
    if (ctx.on_save_config) ctx.on_save_config();
  }
  int pan0 = static_cast<int>(emulator.get(vamiga::Opt::AUD_PAN0));
  int separation = (50 - pan0) * 2;
  if (separation < 0) separation = 0;
  if (separation > 100) separation = 100;
  if (ImGui::SliderInt("Stereo Separation", &separation, 0, 100, "%d%%")) {
      int p_left = 50 - (separation / 2);
      int p_right = 50 + (separation / 2);
      emulator.set(vamiga::Opt::AUD_PAN0, p_left);
      emulator.set(vamiga::Opt::AUD_PAN3, p_left);
      emulator.set(vamiga::Opt::AUD_PAN1, p_right);
      emulator.set(vamiga::Opt::AUD_PAN2, p_right);
  }
  int method = static_cast<int>(emulator.get(vamiga::Opt::AUD_SAMPLING_METHOD));
  static constexpr std::array methods = { "None", "Nearest", "Linear" };
  if (ImGui::Combo("Resampling", &method, methods.data(), methods.size())) {
      emulator.set(vamiga::Opt::AUD_SAMPLING_METHOD, method);
  }
  int buf_size = static_cast<int>(emulator.get(vamiga::Opt::AUD_BUFFER_SIZE));
  if (ImGui::InputInt("Buffer Size", &buf_size, 128, 1024)) {
      if (buf_size < 512) buf_size = 512;
      if (buf_size > 32768) buf_size = 32768;
      emulator.set(vamiga::Opt::AUD_BUFFER_SIZE, buf_size);
  }
  ImGui::TextDisabled("Samples (Lower = less latency, Higher = more stable)");
}
void SettingsWindow::DrawRomInfo(std::string_view label, const vamiga::RomTraits& traits,
                 bool present, std::string* path_buffer,
                 std::function<void()> on_load, std::function<void()> on_eject) {
  ImGui::PushID(label.data());
  ImGui::BeginGroup();
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label.data());
  ImGui::Spacing();
  ImGui::BeginGroup();
  ImVec4 iconColor = present ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, iconColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, iconColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, iconColor);
  ImGui::Button(ICON_FA_MICROCHIP, ImVec2(48, 48));
  ImGui::PopStyleColor(3);
  ImGui::SameLine();
  ImGui::BeginGroup();
  if (present) {
    ImGui::Text("Title:    %s", traits.title ? traits.title : "Unknown");
    ImGui::Text("Revision: %s", traits.revision ? traits.revision : "-");
    ImGui::Text("Released: %s", traits.released ? traits.released : "-");
    ImGui::Text("Model:    %s", traits.model ? traits.model : "-");
    ImGui::TextDisabled("CRC32:    0x%08X", traits.crc);
  } else {
    ImGui::Text("No ROM loaded");
    ImGui::TextDisabled("Load a Kickstart ROM to boot.");
  }
  ImGui::EndGroup();
  ImGui::EndGroup();
  ImGui::Spacing();
  ImGui::Text("Path:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-140);
  ImGui::InputText("##path", path_buffer);
  ImGui::SameLine();
  if (ImGui::Button("Load", ImVec2(60, 0)) && on_load) {
      gui::PickerOptions opts;
      opts.title = std::string("Select ") + std::string(label);
      opts.filters = "ROM Files (*.rom *.bin){.rom,.bin,.adf},All Files (*.*){.*}";
      gui::FilePicker::Instance().Open(
          std::string("RomPicker_") + std::string(label), opts,
          [path_buffer, on_load](std::filesystem::path p) {
            *path_buffer = p.string();
            on_load();
          });
  }
  ImGui::SameLine();
  if (ImGui::Button("Eject", ImVec2(60, 0)) && on_eject) {
      on_eject();
  }
  ImGui::EndGroup();
  ImGui::PopID();
}
}
