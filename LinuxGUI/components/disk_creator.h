#ifndef LINUXGUI_COMPONENTS_DISK_CREATOR_H_
#define LINUXGUI_COMPONENTS_DISK_CREATOR_H_

#include <utility>
#include "VAmiga.h"
#undef unreachable
#define unreachable std::unreachable()
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
    char floppy_label_[32] = "Empty";
    
    int hd_capacity_idx_ = 2; 
    int hd_c_ = 0;
    int hd_h_ = 0;
    int hd_s_ = 0;
    int hd_fs_ = 2; 
    char hd_label_[32] = "System";
    
    void UpdateHDCapacity();
    void CreateFloppy(vamiga::VAmiga& emu);
    void CreateHardDisk(vamiga::VAmiga& emu);
};

}
#endif
