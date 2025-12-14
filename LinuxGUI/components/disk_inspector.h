#ifndef LINUXGUI_COMPONENTS_DISK_INSPECTOR_H_
#define LINUXGUI_COMPONENTS_DISK_INSPECTOR_H_

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include "VAmiga.h"
#include "Media/MediaFile.h"
#include "imgui.h"

namespace gui {

class DiskInspector {
public:
    static DiskInspector& Instance();
    void Draw(vamiga::VAmiga& emu);
    void Open(int drive_nr, bool is_hd, vamiga::VAmiga& emu);

private:
    DiskInspector() = default;
    
    void UpdateMedia(vamiga::VAmiga& emu);
    void UpdateSelectionFromBlock();
    void UpdateSelectionFromGeometry();
    void DrawInfo();
    void DrawNavigation();
    void DrawBlockView();
    void DrawMFMView(vamiga::VAmiga& emu);
    
    bool open_ = false;
    int drive_nr_ = 0;
    bool is_hd_ = false;
    
    std::unique_ptr<vamiga::MediaFile> media_;
    
    int num_cyls_ = 0;
    int num_heads_ = 0;
    int num_sectors_ = 0;
    int num_tracks_ = 0;
    int num_blocks_ = 0;
    
    int current_cyl_ = 0;
    int current_head_ = 0;
    int current_track_ = 0;
    int current_sector_ = 0;
    int current_block_ = 0;
};

}

#endif
