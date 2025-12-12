#include "application.h"
#include <SDL_opengl.h>
#include <format>
#include <print>
#include <ranges>
#include "constants.h"
#include "Ports/AudioPort.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "components/console.h"
#include "components/dashboard.h"
#include "components/disk_creator.h"
#include "components/disk_inspector.h"
#include "components/file_picker.h"
#include "components/inspector.h"
#include "components/settings_window.h"
#include "components/video_window.h"
#include "components/virtual_keyboard.h"
#include "core_actions.h"
#include "imgui.h"
#include "resources/IconsFontAwesome6.h"
#include "resources/font_awesome.h"
Application::Application(int argc, char** argv)
    : gl_context_(nullptr, SDL_GL_DeleteContext) {}
Application::~Application() {
  SaveConfig();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_Quit();
}
bool Application::Init() {
  if (!InitSDL()) return false;
  InitImGui();
  InitEmulator();
  config_ = std::make_unique<gui::ConfigProvider>(emulator_.defaults);
  LoadConfig();
  return true;
}
bool Application::InitSDL() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER |
               SDL_INIT_AUDIO) != 0) {
    std::println(std::cerr, "SDL_Init Error: {}", SDL_GetError());
    return false;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  window_.reset(SDL_CreateWindow(
      "vAmiga Linux", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, gui::kDefaultWindowWidth, gui::kDefaultWindowHeight,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
  if (!window_) {
    std::println(std::cerr, "SDL_CreateWindow Error: {}", SDL_GetError());
    return false;
  }
  gl_context_.reset(SDL_GL_CreateContext(window_.get()));
  if (!gl_context_) {
    std::println(std::cerr, "SDL_GL_CreateContext Error: {}", SDL_GetError());
    return false;
  }
  SDL_GL_MakeCurrent(window_.get(), gl_context_.get());
  SDL_GL_SetSwapInterval(1);
  return true;
}
void Application::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(window_.get(), gl_context_.get());
  ImGui_ImplOpenGL3_Init(gui::kGlslVersion.data());
  io.Fonts->AddFontDefault();
  ImFontConfig config;
  config.MergeMode = true;
  config.PixelSnapH = true;
  config.FontDataOwnedByAtlas = false;
  static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
  io.Fonts->AddFontFromMemoryTTF(
      (void*)FontAwesome_compressed_data, FontAwesome_compressed_size, 13.0f,
      &config, icons_ranges);
  io.Fonts->Build();
}
void Application::InitEmulator() {
  emulator_.set(vamiga::ConfigScheme::A500_OCS_1MB);
  emulator_.set(vamiga::Opt::AMIGA_VSYNC, 0);
  emulator_.set(vamiga::Opt::AUD_ASR, 1);
  emulator_.set(vamiga::Opt::AUD_SAMPLING_METHOD,
                (vamiga::i64)vamiga::SamplingMethod::LINEAR);
  emulator_.set(vamiga::Opt::AUD_BUFFER_SIZE, 16384);
  input_manager_ = std::make_unique<InputManager>(emulator_);
  emulator_.launch();
  SDL_AudioSpec want{}, have{};
  want.freq = gui::kAudioFrequency;
  want.format = AUDIO_F32;
  want.channels = gui::kAudioChannels;
  want.samples = gui::kAudioSamples;
  want.callback = [](void* userdata, Uint8* stream, int len) {
    auto* emu = static_cast<vamiga::VAmiga*>(userdata);
    int num_frames = len / (sizeof(float) * gui::kAudioChannels);
    int copied = emu->audioPort.copyInterleaved(reinterpret_cast<float*>(stream), num_frames);
    if (copied < num_frames) {
      std::fill_n(stream + (copied * sizeof(float) * gui::kAudioChannels),
             (num_frames - copied) * sizeof(float) * gui::kAudioChannels, 0);
    }
  };
  want.userdata = &emulator_;
  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (dev) {
    emulator_.audioPort.port->setSampleRate(have.freq);
    SDL_PauseAudioDevice(dev, 0);
  }
  gui::Console::Instance().SetCommandCallback([this](const std::string& cmd) {
    emulator_.retroShell.press(cmd);
    emulator_.retroShell.press(vamiga::RSKey::RETURN, false);
  });
}
void Application::LoadConfig() {
  config_->Load();
  input_manager_->pause_in_background_ =
      config_->GetBool(gui::ConfigKeys::kPauseBg, true);
  input_manager_->retain_mouse_by_click_ =
      config_->GetBool(gui::ConfigKeys::kRetainClick, true);
  input_manager_->retain_mouse_by_entering_ =
      config_->GetBool(gui::ConfigKeys::kRetainEnter, false);
  input_manager_->release_mouse_by_shaking_ =
      config_->GetBool(gui::ConfigKeys::kShakeRelease, true);
  kickstart_path_ = config_->GetString(gui::ConfigKeys::kKickstartPath);
  ext_rom_path_ = config_->GetString(gui::ConfigKeys::kExtRomPath);
  for (int i : std::views::iota(0, gui::kFloppyDriveCount))
    floppy_paths_[i] = config_->GetFloppyPath(i);
  for (int i : std::views::iota(0, gui::kHardDriveCount)) {
      std::string key = std::format("HD{}Path", i);
      hard_drive_paths_[i] = config_->GetString(key);
      if (!hard_drive_paths_[i].empty()) {
          AttachHardDrive(i, hard_drive_paths_[i]);
      }
  }
  if (!kickstart_path_.empty()) LoadKickstart(kickstart_path_);
  if (!ext_rom_path_.empty()) LoadExtendedRom(ext_rom_path_);
  for (int i : std::views::iota(0, gui::kFloppyDriveCount))
    if (!floppy_paths_[i].empty()) InsertFloppy(i, floppy_paths_[i]);
  emulator_.set(vamiga::Opt::CPU_REVISION, config_->GetInt(gui::ConfigKeys::kHwCpu));
  emulator_.set(vamiga::Opt::AGNUS_REVISION, config_->GetInt(gui::ConfigKeys::kHwAgnus));
  emulator_.set(vamiga::Opt::DENISE_REVISION, config_->GetInt(gui::ConfigKeys::kHwDenise));
  int chip_ram = config_->GetInt(gui::ConfigKeys::kMemChip);
  if (chip_ram > 8192) chip_ram /= 1024;
  emulator_.set(vamiga::Opt::MEM_CHIP_RAM, chip_ram);
  int slow_ram = config_->GetInt(gui::ConfigKeys::kMemSlow);
  if (slow_ram > 8192) slow_ram /= 1024;
  emulator_.set(vamiga::Opt::MEM_SLOW_RAM, slow_ram);
  int fast_ram = config_->GetInt(gui::ConfigKeys::kMemFast);
  if (fast_ram > 8192) fast_ram /= 1024;
  emulator_.set(vamiga::Opt::MEM_FAST_RAM, fast_ram);
  emulator_.set(vamiga::Opt::MON_SCANLINES, config_->GetInt(gui::ConfigKeys::kVidScanlines));
  emulator_.set(vamiga::Opt::MON_SCANLINE_WEIGHT, config_->GetInt(gui::ConfigKeys::kVidScanWeight));
  emulator_.set(vamiga::Opt::MON_BLUR, config_->GetBool(gui::ConfigKeys::kVidBlur));
  emulator_.set(vamiga::Opt::MON_BLUR_RADIUS, config_->GetInt(gui::ConfigKeys::kVidBlurRad));
  emulator_.set(vamiga::Opt::MON_BLOOM, config_->GetBool(gui::ConfigKeys::kVidBloom));
  emulator_.set(vamiga::Opt::MON_BLOOM_WEIGHT, config_->GetInt(gui::ConfigKeys::kVidBloomWgt));
  emulator_.set(vamiga::Opt::MON_DOTMASK, config_->GetInt(gui::ConfigKeys::kVidDotmask));
  emulator_.set(vamiga::Opt::MON_ZOOM, config_->GetInt(gui::ConfigKeys::kVidZoom));
  emulator_.set(vamiga::Opt::MON_CENTER, config_->GetInt(gui::ConfigKeys::kVidCenter));
  emulator_.set(vamiga::Opt::AUD_SAMPLING_METHOD, config_->GetInt(gui::ConfigKeys::kAudSampleMethod));
  emulator_.set(vamiga::Opt::AUD_BUFFER_SIZE, config_->GetInt(gui::ConfigKeys::kAudBufferSize));
  int vol = config_->GetInt(gui::ConfigKeys::kAudioVolume, 100);
  emulator_.set(vamiga::Opt::AUD_VOLL, vol);
  emulator_.set(vamiga::Opt::AUD_VOLR, vol);
  int sep = config_->GetInt(gui::ConfigKeys::kAudioSep, 100);
  int p_left = 50 - (sep / 2);
  int p_right = 50 + (sep / 2);
  emulator_.set(vamiga::Opt::AUD_PAN0, p_left);
  emulator_.set(vamiga::Opt::AUD_PAN3, p_left);
  emulator_.set(vamiga::Opt::AUD_PAN1, p_right);
  emulator_.set(vamiga::Opt::AUD_PAN2, p_right);
  agnus_rev_ = static_cast<int>(emulator_.get(vamiga::Opt::AGNUS_REVISION));
  denise_rev_ = static_cast<int>(emulator_.get(vamiga::Opt::DENISE_REVISION));
  rtc_model_ = static_cast<int>(emulator_.get(vamiga::Opt::RTC_MODEL));
  volume_ = static_cast<int>(emulator_.get(vamiga::Opt::AUD_VOLL));
  current_standard_ = static_cast<int>(emulator_.get(vamiga::Opt::AMIGA_VIDEO_FORMAT));
  is_fullscreen_ = config_->GetBool(gui::ConfigKeys::kUiFullscreen, false);
  if (is_fullscreen_) {
      SDL_SetWindowFullscreen(window_.get(), SDL_WINDOW_FULLSCREEN_DESKTOP);
  }
  scale_mode_ = config_->GetInt(gui::ConfigKeys::kUiScaleMode, 0);
  video_as_background_ = config_->GetBool(gui::ConfigKeys::kUiVideoBack, true);
  show_settings_ = config_->GetBool(gui::ConfigKeys::kUiShowSettings, false);
  show_inspector_ = config_->GetBool(gui::ConfigKeys::kUiShowInspector, false);
  show_dashboard_ = config_->GetBool(gui::ConfigKeys::kUiShowDashboard, false);
  show_console_ = config_->GetBool(gui::ConfigKeys::kUiShowConsole, false);
  show_keyboard_ = config_->GetBool(gui::ConfigKeys::kUiShowKeyboard, false);
  port1_device_ = config_->GetInt(gui::ConfigKeys::kInputPort1, 1);
  port2_device_ = config_->GetInt(gui::ConfigKeys::kInputPort2, 2);
  snapshot_auto_delete_ = config_->GetBool(gui::ConfigKeys::kSnapAutoDelete, gui::Defaults::kSnapshotAutoDelete);
  screenshot_format_ = config_->GetInt(gui::ConfigKeys::kScrnFormat, gui::Defaults::kScreenshotFormat);
  screenshot_source_ = config_->GetInt(gui::ConfigKeys::kScrnSource, gui::Defaults::kScreenshotSource);
}
void Application::SaveConfig() {
  config_->SetBool(gui::ConfigKeys::kPauseBg,
                   input_manager_->pause_in_background_);
  config_->SetBool(gui::ConfigKeys::kRetainClick,
                   input_manager_->retain_mouse_by_click_);
  config_->SetBool(gui::ConfigKeys::kRetainEnter,
                   input_manager_->retain_mouse_by_entering_);
  config_->SetBool(gui::ConfigKeys::kShakeRelease,
                   input_manager_->release_mouse_by_shaking_);
  for (int i : std::views::iota(0, gui::kHardDriveCount)) {
      std::string key = std::format("HD{}Path", i);
      config_->SetString(key, hard_drive_paths_[i]);
  }
  config_->SetBool(gui::ConfigKeys::kUiFullscreen, is_fullscreen_);
  config_->SetInt(gui::ConfigKeys::kUiScaleMode, scale_mode_);
  config_->SetBool(gui::ConfigKeys::kUiVideoBack, video_as_background_);
  config_->SetBool(gui::ConfigKeys::kUiShowSettings, show_settings_);
  config_->SetBool(gui::ConfigKeys::kUiShowInspector, show_inspector_);
  config_->SetBool(gui::ConfigKeys::kUiShowDashboard, show_dashboard_);
  config_->SetBool(gui::ConfigKeys::kUiShowConsole, show_console_);
  config_->SetBool(gui::ConfigKeys::kUiShowKeyboard, show_keyboard_);
  config_->SetInt(gui::ConfigKeys::kInputPort1, port1_device_);
  config_->SetInt(gui::ConfigKeys::kInputPort2, port2_device_);
  config_->SetBool(gui::ConfigKeys::kSnapAutoDelete, snapshot_auto_delete_);
  config_->SetInt(gui::ConfigKeys::kScrnFormat, screenshot_format_);
  config_->SetInt(gui::ConfigKeys::kScrnSource, screenshot_source_);
  config_->SetInt(gui::ConfigKeys::kHwCpu, static_cast<int>(emulator_.get(vamiga::Opt::CPU_REVISION)));
  config_->SetInt(gui::ConfigKeys::kHwAgnus, static_cast<int>(emulator_.get(vamiga::Opt::AGNUS_REVISION)));
  config_->SetInt(gui::ConfigKeys::kHwDenise, static_cast<int>(emulator_.get(vamiga::Opt::DENISE_REVISION)));
  config_->SetInt(gui::ConfigKeys::kMemChip, static_cast<int>(emulator_.get(vamiga::Opt::MEM_CHIP_RAM)));
  config_->SetInt(gui::ConfigKeys::kMemSlow, static_cast<int>(emulator_.get(vamiga::Opt::MEM_SLOW_RAM)));
  config_->SetInt(gui::ConfigKeys::kMemFast, static_cast<int>(emulator_.get(vamiga::Opt::MEM_FAST_RAM)));
  config_->SetInt(gui::ConfigKeys::kVidScanlines, static_cast<int>(emulator_.get(vamiga::Opt::MON_SCANLINES)));
  config_->SetInt(gui::ConfigKeys::kVidScanWeight, static_cast<int>(emulator_.get(vamiga::Opt::MON_SCANLINE_WEIGHT)));
  config_->SetBool(gui::ConfigKeys::kVidBlur, static_cast<bool>(emulator_.get(vamiga::Opt::MON_BLUR)));
  config_->SetInt(gui::ConfigKeys::kVidBlurRad, static_cast<int>(emulator_.get(vamiga::Opt::MON_BLUR_RADIUS)));
  config_->SetBool(gui::ConfigKeys::kVidBloom, static_cast<bool>(emulator_.get(vamiga::Opt::MON_BLOOM)));
  config_->SetInt(gui::ConfigKeys::kVidBloomWgt, static_cast<int>(emulator_.get(vamiga::Opt::MON_BLOOM_WEIGHT)));
  config_->SetInt(gui::ConfigKeys::kVidDotmask, static_cast<int>(emulator_.get(vamiga::Opt::MON_DOTMASK)));
  config_->SetInt(gui::ConfigKeys::kVidZoom, static_cast<int>(emulator_.get(vamiga::Opt::MON_ZOOM)));
  config_->SetInt(gui::ConfigKeys::kVidCenter, static_cast<int>(emulator_.get(vamiga::Opt::MON_CENTER)));
  config_->SetInt(gui::ConfigKeys::kAudSampleMethod, static_cast<int>(emulator_.get(vamiga::Opt::AUD_SAMPLING_METHOD)));
  config_->SetInt(gui::ConfigKeys::kAudBufferSize, static_cast<int>(emulator_.get(vamiga::Opt::AUD_BUFFER_SIZE)));
  config_->SetInt(gui::ConfigKeys::kAudioVolume, static_cast<int>(emulator_.get(vamiga::Opt::AUD_VOLL)));
  int pan0 = static_cast<int>(emulator_.get(vamiga::Opt::AUD_PAN0));
  int separation = (50 - pan0) * 2;
  config_->SetInt(gui::ConfigKeys::kAudioSep, separation);
  config_->Save();
}
void Application::Run() {
  if (!Init()) return;
  MainLoop();
}
void Application::MainLoop() {
  bool done = false;
  while (!done) {
    input_manager_->SetPortDevices(port1_device_, port2_device_);
    input_manager_->Update();
    HandleEvents(done);
    Update();
    Render();
    emulator_.wakeUp();
  }
}
void Application::HandleEvents(bool& done) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    if (event.type == SDL_QUIT) done = true;
    if (event.type == SDL_WINDOWEVENT) {
      if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window_.get()))
        done = true;
      if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
        input_manager_->HandleWindowFocus(true);
      if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        input_manager_->HandleWindowFocus(false);
    }
    if (event.type == SDL_DROPFILE) {
      std::filesystem::path path(event.drop.file);
      std::string ext = path.extension().string();
      for (auto& c : ext) c = tolower(c);
      if (ext == ".adf" || ext == ".adz" || ext == ".dms" || ext == ".ipf")
        InsertFloppy(0, path);
      else if (ext == ".rom" || ext == ".bin")
        LoadKickstart(path);
      else if (ext == ".vsn")
        LoadSnapshot(path);
      SDL_free(event.drop.file);
    }
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE && is_fullscreen_) {
            ToggleFullscreen();
            continue;
        }
        if (event.key.keysym.sym == SDLK_F12) {
            show_ui_ = !show_ui_;
            continue;
        }
        if (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)) {
            ToggleFullscreen();
            continue;
        }
    }
    input_manager_->HandleEvent(event);
  }
}
void Application::Update() {}
void Application::Render() {
  if (video_texture_ == 0) {
    glGenTextures(1, &video_texture_);
    glBindTexture(GL_TEXTURE_2D, video_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_2D, video_texture_);
  GLint filter = (filter_mode_ == 0) ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  emulator_.videoPort.lockTexture();
  const uint32_t* pixels = emulator_.videoPort.getTexture();
  if (pixels) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vamiga::HPIXELS, vamiga::VPIXELS, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  }
  emulator_.videoPort.unlockTexture();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  if (show_ui_) {
      DrawGUI();
  }
  if (video_as_background_) {
      DrawVideoBackground(video_texture_);
  }
  ImGui::Render();
  ImGuiIO& io = ImGui::GetIO();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window_.get());
}
void Application::DrawVideoBackground(uint32_t texture_id) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImVec2 pos = viewport->WorkPos;
    ImVec2 size = viewport->WorkSize;
    float aspect_amiga = (float)vamiga::HPIXELS / (float)vamiga::VPIXELS;
    float aspect_window = size.x / size.y;
    float w = size.x;
    float h = size.y;
    if (scale_mode_ == 1) {
    } else if (scale_mode_ == 2) {
        float scale = 1.0f;
        while ((vamiga::HPIXELS * (scale + 1) <= size.x) && (vamiga::VPIXELS * (scale + 1) <= size.y)) {
            scale += 1.0f;
        }
        w = vamiga::HPIXELS * scale;
        h = vamiga::VPIXELS * scale;
    } else {
        if (aspect_window > aspect_amiga) {
            w = h * aspect_amiga;
        } else {
            h = w / aspect_amiga;
        }
    }
    float x = pos.x + (size.x - w) * 0.5f;
    float y = pos.y + (size.y - h) * 0.5f;
    draw_list->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(texture_id)),
        ImVec2(x, y),
        ImVec2(x + w, y + h));
    if (ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + w, y + h)) && !ImGui::GetIO().WantCaptureMouse) {
        input_manager_->SetViewportHovered(true);
    } else {
        input_manager_->SetViewportHovered(false);
    }
}
void Application::DrawGUI() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Import Script...")) {
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Quit", "Alt+F4")) {
        SDL_Event q;
        q.type = SDL_QUIT;
        SDL_PushEvent(&q);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Machine")) {
      ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
      ImGui::MenuItem("Dashboard", nullptr, &show_dashboard_);
      ImGui::MenuItem("Console", nullptr, &show_console_);
      ImGui::Separator();
      if (ImGui::MenuItem("Load Snapshot...")) {
        gui::PickerOptions opts;
        opts.title = "Open Snapshot";
        opts.filters = "Snapshot Files (*.vsn){.vsn}";
        gui::FilePicker::Instance().Open("SnapLoad", opts, [this](auto p) {
          LoadSnapshot(p);
        });
      }
      if (ImGui::MenuItem("Save Snapshot...")) {
        gui::PickerOptions opts;
        opts.title = "Save Snapshot";
        opts.mode = gui::PickerMode::kSaveFile;
        opts.filters = "Snapshot Files (*.vsn){.vsn}";
        gui::FilePicker::Instance().Open("SnapSave", opts, [this](auto p) {
          SaveSnapshot(p);
        });
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Take Screenshot")) {
          TakeScreenshot();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem(emulator_.isRunning() ? "Pause" : "Continue")) {
            ToggleRunPause();
        }
        if (ImGui::MenuItem("Reset")) {
            HardReset();
        }
        if (ImGui::MenuItem(emulator_.isPoweredOn() ? "Power Off" : "Power On")) {
            TogglePower();
        }
        ImGui::Separator();
        ImGui::MenuItem("Settings", nullptr, &show_settings_);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Fullscreen", "Alt+Enter", &is_fullscreen_)) {
            ToggleFullscreen();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Keyboard")) {
        ImGui::MenuItem("Virtual Keyboard", nullptr, &show_keyboard_);
        ImGui::EndMenu();
    }
    for (int i : std::views::iota(0, gui::kFloppyDriveCount)) {
        if (emulator_.df[i]->getInfo().isConnected) DrawDriveMenu(i);
    }
    for (int i : std::views::iota(0, gui::kHardDriveCount)) {
        if (emulator_.hd[i]->getInfo().isConnected) DrawHardDriveMenu(i);
    }

    ImGui::EndMainMenuBar();
  }
  DrawToolbar();
  if (show_settings_) {
    gui::SettingsContext ctx;
    ctx.kickstart_path = &kickstart_path_;
    ctx.ext_rom_path = &ext_rom_path_;
    for (int i : std::views::iota(0, gui::kFloppyDriveCount)) ctx.floppy_paths[i] = &floppy_paths_[i];
    for (int i : std::views::iota(0, gui::kHardDriveCount)) ctx.hard_drive_paths[i] = &hard_drive_paths_[i];
    ctx.pause_in_background = &input_manager_->pause_in_background_;
    ctx.retain_mouse_by_click = &input_manager_->retain_mouse_by_click_;
    ctx.retain_mouse_by_entering = &input_manager_->retain_mouse_by_entering_;
    ctx.release_mouse_by_shaking = &input_manager_->release_mouse_by_shaking_;
    ctx.volume = &volume_;
    ctx.scale_mode = &scale_mode_;
    ctx.is_fullscreen = &is_fullscreen_;
    ctx.video_as_background = &video_as_background_;
    ctx.snapshot_auto_delete = &snapshot_auto_delete_;
    ctx.screenshot_format = &screenshot_format_;
    ctx.screenshot_source = &screenshot_source_;
    ctx.on_load_kickstart = [this](auto p) { LoadKickstart(p); };
    ctx.on_eject_kickstart = [this]() { EjectKickstart(); };
    ctx.on_load_ext_rom = [this](auto p) { LoadExtendedRom(p); };
    ctx.on_eject_ext_rom = [this]() { EjectExtendedRom(); };
    ctx.on_insert_floppy = [this](int i, auto p) { InsertFloppy(i, p); };
    ctx.on_eject_floppy = [this](int i) { EjectFloppy(i); };
    ctx.on_attach_hd = [this](int i, auto p) { AttachHardDrive(i, p); };
    ctx.on_detach_hd = [this](int i) { DetachHardDrive(i); };
    ctx.on_save_config = [this]() { SaveConfig(); };
    ctx.on_toggle_fullscreen = [this]() { ToggleFullscreen(); };
    gui::SettingsWindow::Instance().Draw(&show_settings_, emulator_, ctx);
  }
  if (!video_as_background_) {
      bool open = true;
      bool hovered = gui::VideoWindow::Instance().Draw(&open, video_texture_, scale_mode_);
      if (hovered) {
          input_manager_->SetViewportHovered(true);
      } else {
          input_manager_->SetViewportHovered(false);
      }
  }
  if (show_inspector_)
    gui::Inspector::Instance().Draw(&show_inspector_, emulator_);
  if (show_dashboard_)
    gui::Dashboard::Instance().Draw(&show_dashboard_, emulator_);
  if (show_console_) gui::Console::Instance().Draw(&show_console_, emulator_);
  if (show_keyboard_)
    gui::VirtualKeyboard::Instance().Draw(&show_keyboard_, emulator_);
  gui::DiskCreator::Instance().Draw(emulator_);
  gui::DiskInspector::Instance().Draw(emulator_);
  gui::FilePicker::Instance().Draw();
}
void Application::DrawToolbar() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight()));
  ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 40));
  ImGui::Begin("Toolbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::BeginGroup();
  if (ImGui::Button(ICON_FA_GEAR)) {
    show_settings_ = !show_settings_;
  }
  ImGui::SetItemTooltip("Settings");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS)) {
    show_inspector_ = !show_inspector_;
  }
  ImGui::SetItemTooltip("Inspector");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_GAUGE_HIGH)) {
    show_dashboard_ = !show_dashboard_;
  }
  ImGui::SetItemTooltip("Dashboard");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_TERMINAL)) {
    show_console_ = !show_console_;
  }
  ImGui::SetItemTooltip("Console");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_KEYBOARD)) {
    show_keyboard_ = !show_keyboard_;
  }
  ImGui::SetItemTooltip("Virtual Keyboard");
  ImGui::EndGroup();
  ImGui::SameLine(0, 20.0f);
  ImGui::BeginGroup();
  if (ImGui::Button(ICON_FA_DOWNLOAD)) {
    gui::PickerOptions opts;
    opts.title = "Save Snapshot";
    opts.mode = gui::PickerMode::kSaveFile;
    opts.filters = "Snapshot Files (*.vsn){.vsn}";
    gui::FilePicker::Instance().Open("SnapSaveBtn", opts, [this](auto p) {
      SaveSnapshot(p);
    });
  }
  ImGui::SetItemTooltip("Save Snapshot");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_UPLOAD)) {
    gui::PickerOptions opts;
    opts.title = "Open Snapshot";
    opts.filters = "Snapshot Files (*.vsn){.vsn}";
    gui::FilePicker::Instance().Open("SnapLoadBtn", opts, [this](auto p) {
      LoadSnapshot(p);
    });
  }
  ImGui::SetItemTooltip("Load Snapshot");
  ImGui::EndGroup();
  ImGui::SameLine(0, 20.0f);
  ImGui::BeginGroup();
  ImGui::PushID("Port1Btn");
  if (ImGui::Button(GetDeviceIcon(port1_device_).data())) {
    ImGui::OpenPopup("Port1Menu");
  }
  ImGui::SetItemTooltip(std::format("Port 1: {}", InputManager::GetDeviceName(port1_device_)).c_str());
  ImGui::PopID();
  DrawPortDeviceSelection(0, &port1_device_);
  ImGui::SameLine();
  ImGui::PushID("Port2Btn");
  if (ImGui::Button(GetDeviceIcon(port2_device_).data())) {
    ImGui::OpenPopup("Port2Menu");
  }
  ImGui::SetItemTooltip(std::format("Port 2: {}", InputManager::GetDeviceName(port2_device_)).c_str());
  ImGui::PopID();
  DrawPortDeviceSelection(1, &port2_device_);
  ImGui::EndGroup();
  ImGui::SameLine(0, 20.0f);
  ImGui::BeginGroup();
  if (ImGui::Button(emulator_.isRunning() ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
    ToggleRunPause();
  }
  ImGui::SetItemTooltip(emulator_.isRunning() ? "Pause" : "Run");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_ROTATE_RIGHT)) {
    HardReset();
  }
  ImGui::SetItemTooltip("Hard Reset");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_POWER_OFF)) {
    TogglePower();
  }
  ImGui::SetItemTooltip(emulator_.isPoweredOn() ? "Power Off" : "Power On");
  ImGui::EndGroup();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);
  ImGui::End();
}
std::string_view Application::GetDeviceIcon(int device_id) {
    switch (device_id) {
        case 0: return ICON_FA_BAN;
        case 1: return ICON_FA_COMPUTER_MOUSE;
        case 2:
        case 3: return ICON_FA_KEYBOARD;
        case 4:
        case 5:
        case 6:
        case 7: return ICON_FA_GAMEPAD;
        default: return ICON_FA_QUESTION;
    }
}
void Application::DrawPortDeviceSelection(int port_idx, int* device_id) {
    std::string popup_id = std::format("Port{}Menu", port_idx + 1);
    if (ImGui::BeginPopup(popup_id.c_str())) {
        static constexpr std::array devices = {"None", "Mouse", "Keyset 1", "Keyset 2", "Gamepad 1", "Gamepad 2", "Gamepad 3", "Gamepad 4"};
        for (int i : std::views::iota(0, (int)devices.size())) {
            std::string label = std::format("{} {}", GetDeviceIcon(i), devices[i]);
            if (ImGui::Selectable(label.c_str(), *device_id == i)) {
                *device_id = i;
                input_manager_->SetPortDevices(port1_device_, port2_device_);
            }
        }
        ImGui::EndPopup();
    }
}
void Application::LoadKickstart(const std::filesystem::path& path) {
  try {
    emulator_.mem.loadRom(path);
    emulator_.hardReset();
    config_->SetString(gui::ConfigKeys::kKickstartPath, path.string());
    kickstart_path_ = path.string();
  } catch (...) {
  }
}
void Application::EjectKickstart() {
  emulator_.mem.deleteRom();
  emulator_.hardReset();
  config_->SetString(gui::ConfigKeys::kKickstartPath, "");
  kickstart_path_ = "";
}
void Application::LoadExtendedRom(const std::filesystem::path& path) {
  try {
    emulator_.mem.loadExt(path);
    emulator_.hardReset();
    config_->SetString(gui::ConfigKeys::kExtRomPath, path.string());
    ext_rom_path_ = path.string();
  } catch (...) {
  }
}
void Application::EjectExtendedRom() {
  emulator_.mem.deleteExt();
  emulator_.hardReset();
  config_->SetString(gui::ConfigKeys::kExtRomPath, "");
  ext_rom_path_ = "";
}
void Application::InsertFloppy(int drive, const std::filesystem::path& path) {
  if (drive < 0 || drive > 3) return;
  try {
    emulator_.df[drive]->insert(path, false);
    config_->SetFloppyPath(drive, path.string());
    floppy_paths_[drive] = path.string();
  } catch (...) {
  }
}
void Application::EjectFloppy(int drive) {
  if (drive < 0 || drive > 3) return;
  emulator_.df[drive]->ejectDisk();
  config_->SetFloppyPath(drive, "");
  floppy_paths_[drive] = "";
}
void Application::AttachHardDrive(int drive, const std::filesystem::path& path) {
  if (drive < 0 || drive > 3) return;
  try {
      emulator_.hd[drive]->attach(path);
      std::string key = std::format("HD{}Path", drive);
      config_->SetString(key, path.string());
      hard_drive_paths_[drive] = path.string();
  } catch(...) {}
}
void Application::DetachHardDrive(int drive) {
    if (drive < 0 || drive > 3) return;
    std::string key = std::format("HD{}Path", drive);
    config_->SetString(key, "");
    hard_drive_paths_[drive] = "";
}
void Application::TogglePower() {
  if (emulator_.isPoweredOn())
    emulator_.powerOff();
  else
    emulator_.run();
}
void Application::HardReset() { emulator_.hardReset(); }
void Application::ToggleRunPause() {
  if (emulator_.isRunning())
    emulator_.pause();
  else
    emulator_.run();
}
void Application::LoadSnapshot(const std::filesystem::path& path) {
  try {
    emulator_.amiga.loadSnapshot(path);
  } catch (...) {
  }
}
void Application::SaveSnapshot(const std::filesystem::path& path) {
  try {
    emulator_.amiga.saveSnapshot(path);
    ManageSnapshots();
  } catch (...) {
  }
}
void Application::ManageSnapshots() {
}
void Application::TakeScreenshot() {
    int w = vamiga::HPIXELS;
    int h = vamiga::VPIXELS;
    std::vector<uint32_t> pixels(w * h);
    
    if (video_texture_ != 0) {
        glBindTexture(GL_TEXTURE_2D, video_texture_);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        
        Uint32 rmask = 0x000000ff;
        Uint32 gmask = 0x0000ff00;
        Uint32 bmask = 0x00ff0000;
        Uint32 amask = 0xff000000;
        
        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels.data(), w, h, 32, w * 4, rmask, gmask, bmask, amask);
        if (surface) {
            namespace fs = std::filesystem;
            const char* home = std::getenv("HOME");
            fs::path dir = home ? fs::path(home) / gui::Defaults::kConfigDir / gui::Defaults::kAppName / gui::Defaults::kScreenshotsDir : fs::path("screenshots");
            std::error_code ec;
            if (!fs::exists(dir)) fs::create_directories(dir, ec);
            
            for (int i : std::views::iota(0, 1000)) {
                std::string name = std::format("{:03}.bmp", i);
                fs::path p = dir / name;
                if (!fs::exists(p)) {
                    SDL_SaveBMP(surface, p.string().c_str());
                    break;
                }
            }
            SDL_FreeSurface(surface);
        }
    }
}
void Application::DrawDriveMenu(int drive_index) {
    if (ImGui::BeginMenu(std::format("DF{}", drive_index).c_str())) {
        if (ImGui::MenuItem("New Disk...")) {
            gui::DiskCreator::Instance().OpenForFloppy(drive_index);
        }
        if (ImGui::MenuItem("Insert Disk...")) {
            gui::PickerOptions opts;
            opts.title = "Insert Disk";
            opts.filters = "Amiga Disk Files (*.adf,*.adz,*.dms,*.ipf){.adf,.adz,.dms,.ipf}";
            gui::FilePicker::Instance().Open(std::format("InsertDF{}", drive_index), opts, [this, drive_index](auto p) {
                InsertFloppy(drive_index, p);
            });
        }
        if (ImGui::MenuItem("Eject Disk")) {
            EjectFloppy(drive_index);
        }
        if (ImGui::MenuItem("Inspect Disk...")) {
            gui::DiskInspector::Instance().Open(drive_index, false, emulator_);
        }
        ImGui::EndMenu();
    }
}

void Application::DrawHardDriveMenu(int drive_index) {
    if (ImGui::BeginMenu(std::format("HD{}", drive_index).c_str())) {
        if (ImGui::MenuItem("New Hard Disk...")) {
            gui::DiskCreator::Instance().OpenForHardDisk(drive_index);
        }
        if (ImGui::MenuItem("Attach Hard Drive...")) {
            gui::PickerOptions opts;
            opts.title = "Attach Hard Drive";
            opts.filters = "Hard Disk Files (*.hdf,*.hdz){.hdf,.hdz}";
            gui::FilePicker::Instance().Open(std::format("AttachHD{}", drive_index), opts, [this, drive_index](auto p) {
                AttachHardDrive(drive_index, p);
            });
        }
        if (ImGui::MenuItem("Detach Hard Drive")) {
            DetachHardDrive(drive_index);
        }
        if (ImGui::MenuItem("Inspect Disk...")) {
            gui::DiskInspector::Instance().Open(drive_index, true, emulator_);
        }
        ImGui::EndMenu();
    }
}

void Application::ToggleFullscreen() {
    is_fullscreen_ = !is_fullscreen_;
    if (is_fullscreen_) {
        SDL_SetWindowFullscreen(window_.get(), SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(window_.get(), 0);
    }
}
namespace core_actions {
std::string_view GetRetroShellText() { return ""; }
void RetroShellPressKey(vamiga::RSKey key, bool shift) {}
void RetroShellPressChar(char c) {}
void RetroShellPressString(const std::string& s) {}
}
