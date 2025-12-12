#ifndef LINUXGUI_CONSTANTS_H_
#define LINUXGUI_CONSTANTS_H_

#include <cstddef>
#include <string_view>

namespace gui {

static constexpr int kDefaultWindowWidth = 1280;
static constexpr int kDefaultWindowHeight = 720;

static constexpr int kAudioFrequency = 44100;
static constexpr int kAudioChannels = 2;
static constexpr int kAudioSamples = 1024;

static constexpr int kFloppyDriveCount = 4;
static constexpr int kHardDriveCount = 4;

static constexpr std::string_view kGlslVersion = "#version 130";

namespace Defaults {
    static constexpr std::string_view kConfigDir = ".config";
    static constexpr std::string_view kAppName = "vamiga";
    static constexpr std::string_view kConfigFileName = "vamiga.config";
    static constexpr std::string_view kScreenshotsDir = "screenshots";
    static constexpr std::string_view kSnapshotsDir = "snapshots";
    
    static constexpr bool kPauseInBackground = true;
    static constexpr bool kRetainMouseClick = true;
    static constexpr bool kRetainMouseEnter = false;
    static constexpr bool kReleaseMouseShake = true;
    
    static constexpr int kAudioVolume = 100;
    static constexpr int kAudioSeparation = 100;
    static constexpr int kAudioBufferSize = 16384;
    static constexpr int kAudioSampleMethod = 2;
    
    static constexpr int kMemChip = 512;
    static constexpr int kMemSlow = 512;
    static constexpr int kMemFast = 0;
    
    static constexpr int kVidZoom = 0;
    static constexpr int kVidCenter = 0;

    static constexpr bool kSnapshotAutoDelete = true;
    static constexpr int kSnapshotLimit = 100;
    static constexpr int kScreenshotFormat = 0;
    static constexpr int kScreenshotSource = 0;
}

}

#endif