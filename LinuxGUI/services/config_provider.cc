#include "config_provider.h"
#include <charconv>
#include <cstdlib>
#include <format>
#include <iostream>
#include <ranges>
#include "constants.h"
#include "Components/Agnus/AgnusTypes.h"
#include "Components/CPU/CPUTypes.h"
#include "Components/Denise/DeniseTypes.h"

namespace gui {

ConfigProvider::ConfigProvider(vamiga::DefaultsAPI& defaults_api)
    : defaults_(defaults_api) {
  defaults_.setFallback(std::string(ConfigKeys::kKickstartPath), "");
  defaults_.setFallback(std::string(ConfigKeys::kExtRomPath), "");
  
  for (int i : std::views::iota(0, gui::kFloppyDriveCount)) {
    defaults_.setFallback(std::format("DF{{}}Path", i), "");
  }
  for (int i : std::views::iota(0, gui::kHardDriveCount)) {
    defaults_.setFallback(std::format("HD{{}}Path", i), "");
  }
  
  defaults_.setFallback(std::string(ConfigKeys::kPauseBg), std::to_string(Defaults::kPauseInBackground));
  defaults_.setFallback(std::string(ConfigKeys::kRetainClick), std::to_string(Defaults::kRetainMouseClick));
  defaults_.setFallback(std::string(ConfigKeys::kRetainEnter), std::to_string(Defaults::kRetainMouseEnter));
  defaults_.setFallback(std::string(ConfigKeys::kShakeRelease), std::to_string(Defaults::kReleaseMouseShake));
  
  defaults_.setFallback(std::string(ConfigKeys::kAudioVolume), std::to_string(Defaults::kAudioVolume));
  defaults_.setFallback(std::string(ConfigKeys::kAudioSep), std::to_string(Defaults::kAudioSeparation));
  
  defaults_.setFallback(std::string(ConfigKeys::kUiFullscreen), "0");
  defaults_.setFallback(std::string(ConfigKeys::kUiScaleMode), "0");
  defaults_.setFallback(std::string(ConfigKeys::kUiVideoBack), "1");
  defaults_.setFallback(std::string(ConfigKeys::kUiShowSettings), "0");
  defaults_.setFallback(std::string(ConfigKeys::kUiShowInspector), "0");
  defaults_.setFallback(std::string(ConfigKeys::kUiShowDashboard), "0");
  defaults_.setFallback(std::string(ConfigKeys::kUiShowConsole), "0");
  defaults_.setFallback(std::string(ConfigKeys::kUiShowKeyboard), "0");
  defaults_.setFallback(std::string(ConfigKeys::kInputPort1), "1");
  defaults_.setFallback(std::string(ConfigKeys::kInputPort2), "2");
   
  defaults_.setFallback(std::string(ConfigKeys::kHwCpu), std::to_string(static_cast<int>(vamiga::CPURev::CPU_68000)));
  defaults_.setFallback(std::string(ConfigKeys::kHwAgnus), std::to_string(static_cast<int>(vamiga::AgnusRevision::OCS)));
  defaults_.setFallback(std::string(ConfigKeys::kHwDenise), std::to_string(static_cast<int>(vamiga::DeniseRev::OCS)));
  
  defaults_.setFallback(std::string(ConfigKeys::kMemChip), std::to_string(Defaults::kMemChip));
  defaults_.setFallback(std::string(ConfigKeys::kMemSlow), std::to_string(Defaults::kMemSlow));
  defaults_.setFallback(std::string(ConfigKeys::kMemFast), std::to_string(Defaults::kMemFast));
   
  defaults_.setFallback(std::string(ConfigKeys::kVidScanlines), "0");
  defaults_.setFallback(std::string(ConfigKeys::kVidScanWeight), "0");
  defaults_.setFallback(std::string(ConfigKeys::kVidBlur), "0");
  defaults_.setFallback(std::string(ConfigKeys::kVidBlurRad), "0");
  defaults_.setFallback(std::string(ConfigKeys::kVidBloom), "0");
  defaults_.setFallback(std::string(ConfigKeys::kVidBloomWgt), "0");
  defaults_.setFallback(std::string(ConfigKeys::kVidDotmask), "0");
  defaults_.setFallback(std::string(ConfigKeys::kVidZoom), std::to_string(Defaults::kVidZoom));  
  defaults_.setFallback(std::string(ConfigKeys::kVidCenter), std::to_string(Defaults::kVidCenter));  
   
  defaults_.setFallback(std::string(ConfigKeys::kAudSampleMethod), std::to_string(Defaults::kAudioSampleMethod));  
  defaults_.setFallback(std::string(ConfigKeys::kAudBufferSize), std::to_string(Defaults::kAudioBufferSize));

  defaults_.setFallback(std::string(ConfigKeys::kSnapAutoDelete), std::to_string(Defaults::kSnapshotAutoDelete));
  defaults_.setFallback(std::string(ConfigKeys::kScrnFormat), std::to_string(Defaults::kScreenshotFormat));
  defaults_.setFallback(std::string(ConfigKeys::kScrnSource), std::to_string(Defaults::kScreenshotSource));
}
std::filesystem::path ConfigProvider::GetConfigPath() const {
  const char* home = std::getenv("HOME");
  std::filesystem::path config_dir = home ? std::filesystem::path(home) / Defaults::kConfigDir / Defaults::kAppName : ".";
  std::error_code ec;

  if (!std::filesystem::exists(config_dir, ec)) {
    std::filesystem::create_directories(config_dir, ec);
  }
  return config_dir / Defaults::kConfigFileName;
}

void ConfigProvider::Load() {
  try {
    std::filesystem::path path = GetConfigPath();
    if (std::filesystem::exists(path)) {
      defaults_.load(path);
    }
  } catch (const std::exception& e) {
    std::cerr << std::format("Config Load Error: {{}}\n", e.what());
  } catch (...) {
    std::cerr << "Config Load Error: Unknown\n";
  }
}

void ConfigProvider::Save() {
  try {
    defaults_.save(GetConfigPath());
  } catch (const std::exception& e) {
    std::cerr << std::format("Config Save Error: {{}}\n", e.what());
  } catch (...) {
  }
}

std::string ConfigProvider::GetString(std::string_view key,
                                      const std::string& fallback) {
  try {
    std::string val = defaults_.getRaw(std::string(key));
    return val.empty() ? fallback : val;
  } catch (...) {
    return fallback;
  }
}

void ConfigProvider::SetString(std::string_view key, const std::string& value) {
  try {
    defaults_.set(std::string(key), value);
  } catch (...) {
  }
}

bool ConfigProvider::GetBool(std::string_view key, bool fallback) {
  std::string val = GetString(key, "");
  if (val == "1") return true;
  if (val == "0") return false;
  return fallback;
}

void ConfigProvider::SetBool(std::string_view key, bool value) {
  SetString(key, value ? "1" : "0");
}

int ConfigProvider::GetInt(std::string_view key, int fallback) {
  std::string val = GetString(key, "");
  if (val.empty()) return fallback;
  int result = 0;
  auto [ptr, ec] =
      std::from_chars(val.data(), val.data() + val.size(), result);
  if (ec == std::errc()) {
    return result;
  }
  return fallback;
}

void ConfigProvider::SetInt(std::string_view key, int value) {
  SetString(key, std::to_string(value));
}

std::string ConfigProvider::GetFloppyPath(int drive) {
  return GetString(std::format("DF{{}}Path", drive));
}

void ConfigProvider::SetFloppyPath(int drive, const std::string& path) {
  SetString(std::format("DF{{}}Path", drive), path);
}

}
