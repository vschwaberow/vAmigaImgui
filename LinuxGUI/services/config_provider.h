#ifndef LINUXGUI_SERVICES_CONFIG_PROVIDER_H_
#define LINUXGUI_SERVICES_CONFIG_PROVIDER_H_
#include <filesystem>
#include <string>
#include <utility>
#include <string_view>
#include "VAmiga.h"
#undef unreachable
#define unreachable std::unreachable()
namespace gui {
struct ConfigKeys {
  static constexpr std::string_view kKickstartPath = "KickstartPath";
  static constexpr std::string_view kExtRomPath    = "ExtRomPath";
  static constexpr std::string_view kPauseBg       = "Input.PauseInBackground";
  static constexpr std::string_view kRetainClick   = "Input.RetainMouseByClick";
  static constexpr std::string_view kRetainEnter   = "Input.RetainMouseByEntering";
  static constexpr std::string_view kShakeRelease  = "Input.ReleaseMouseByShaking";
  static constexpr std::string_view kAudioVolume   = "Audio.Volume";
  static constexpr std::string_view kAudioSep      = "Audio.Separation";
  static constexpr std::string_view kUiFullscreen    = "UI.Fullscreen";
  static constexpr std::string_view kUiScaleMode     = "UI.ScaleMode";
  static constexpr std::string_view kUiVideoBack     = "UI.VideoAsBackground";
  static constexpr std::string_view kUiShowSettings  = "UI.ShowSettings";
  static constexpr std::string_view kUiShowInspector = "UI.ShowInspector";
  static constexpr std::string_view kUiShowDashboard = "UI.ShowDashboard";
  static constexpr std::string_view kUiShowConsole   = "UI.ShowConsole";
  static constexpr std::string_view kUiShowKeyboard  = "UI.ShowKeyboard";
  static constexpr std::string_view kInputPort1 = "Input.Port1Device";
  static constexpr std::string_view kInputPort2 = "Input.Port2Device";
   
  static constexpr std::string_view kHwCpu       = "Hardware.CPU";
  static constexpr std::string_view kHwAgnus     = "Hardware.Agnus";
  static constexpr std::string_view kHwDenise    = "Hardware.Denise";
  static constexpr std::string_view kMemChip     = "Memory.Chip";
  static constexpr std::string_view kMemSlow     = "Memory.Slow";
  static constexpr std::string_view kMemFast     = "Memory.Fast";
   
  static constexpr std::string_view kVidScanlines = "Video.Scanlines";
  static constexpr std::string_view kVidScanWeight= "Video.ScanlineWeight";
  static constexpr std::string_view kVidBlur      = "Video.Blur";
  static constexpr std::string_view kVidBlurRad   = "Video.BlurRadius";
  static constexpr std::string_view kVidBloom     = "Video.Bloom";
  static constexpr std::string_view kVidBloomWgt  = "Video.BloomWeight";
  static constexpr std::string_view kVidDotmask   = "Video.Dotmask";
  static constexpr std::string_view kVidZoom      = "Video.Zoom";
  static constexpr std::string_view kVidCenter    = "Video.Center";
   
  static constexpr std::string_view kAudSampleMethod = "Audio.SamplingMethod";
  static constexpr std::string_view kAudBufferSize   = "Audio.BufferSize";

  static constexpr std::string_view kSnapAutoDelete  = "Snapshot.AutoDelete";
  static constexpr std::string_view kScrnFormat      = "Screenshot.Format";
  static constexpr std::string_view kScrnSource      = "Screenshot.Source";
};
class ConfigProvider {
 public:
  explicit ConfigProvider(vamiga::DefaultsAPI& defaults_api);
  void Load();
  void Save();
  std::string GetString(std::string_view key, const std::string& fallback = "");
  void SetString(std::string_view key, const std::string& value);
  bool GetBool(std::string_view key, bool fallback = false);
  void SetBool(std::string_view key, bool value);
  int GetInt(std::string_view key, int fallback = 0);
  void SetInt(std::string_view key, int value);
  std::string GetFloppyPath(int drive);
  void SetFloppyPath(int drive, const std::string& path);
 private:
  std::filesystem::path GetConfigPath() const;
  vamiga::DefaultsAPI& defaults_;
};
}
#endif
