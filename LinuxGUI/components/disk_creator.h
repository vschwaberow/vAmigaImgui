#ifndef LINUXGUI_COMPONENTS_DISK_CREATOR_H_
#define LINUXGUI_COMPONENTS_DISK_CREATOR_H_

#include <utility>
#include "VAmiga.h"
#include <vector>
#include "../compat.h"

#ifndef unreachable
#define unreachable std::unreachable()
#endif
#include "imgui.h"
#include <string>

namespace gui {

class DiskCreator {
public:
    static DiskCreator& Instance();
    void Draw(vamiga::VAmiga& emu);
    void OpenForFloppy(int drive_nr);
    void OpenForHardDisk(int drive_nr);

private:
    DiskCreator();
    
    bool open_ = false;
    bool is_hard_disk_ = false;
    int target_drive_nr_ = 0;
    
    int floppy_type_ = 0; 
    int floppy_fs_ = 0; 
    int floppy_boot_ = 1; 
    std::string floppy_label_ = "Empty";
    
    int hd_capacity_idx_ = 2; 
    int hd_c_ = 0;
    int hd_h_ = 0;
    int hd_s_ = 0;
    int hd_fs_ = 2; 
    std::string hd_label_ = "System";
    
    void UpdateHDCapacity();
    void CreateFloppy(vamiga::VAmiga& emu);
    void CreateHardDisk(vamiga::VAmiga& emu);
};

}
#endif
