#include "disk_creator.h"
#include <format>
#include "FloppyDriveTypes.h"
#include "HardDriveTypes.h"
#include "FSTypes.h"
#include "BootBlockImageTypes.h"

namespace ImGui {
inline bool InputText(const char* label, std::string* str,
                      ImGuiInputTextFlags flags = 0) {
  return InputText(
      label, str->data(), str->capacity() + 1,
      flags | ImGuiInputTextFlags_CallbackResize,
      [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
          auto* s = static_cast<std::string*>(data->UserData);
          s->resize(static_cast<size_t>(data->BufTextLen));
          data->Buf = s->data();
        }
        return 0;
      },
      str);
}
}  // namespace ImGui

namespace gui {

DiskCreator& DiskCreator::Instance() {
    static DiskCreator instance;
    return instance;
}

DiskCreator::DiskCreator() {
    UpdateHDCapacity();
}

void DiskCreator::OpenForFloppy(int drive_nr) {
    target_drive_nr_ = drive_nr;
    is_hard_disk_ = false;
    floppy_fs_ = 0; 
    floppy_boot_ = 1; 
    floppy_label_ = "Empty";
    open_ = true;
}

void DiskCreator::OpenForHardDisk(int drive_nr) {
    target_drive_nr_ = drive_nr;
    is_hard_disk_ = true;
    UpdateHDCapacity();
    hd_label_ = "System";
    open_ = true;
}

void DiskCreator::UpdateHDCapacity() {
    int mb = 0;
    switch(hd_capacity_idx_) {
        case 0: mb = 10; break;
        case 1: mb = 20; break;
        case 2: mb = 40; break;
        case 3: mb = 80; break;
    }
    if (mb > 0) {
        int bsize = 512;
        hd_s_ = 32;
        hd_h_ = 1;
        hd_c_ = (mb * 1024 * 1024) / (hd_h_ * hd_s_ * bsize);
        while (hd_c_ > 1024) {
            hd_c_ /= 2;
            hd_h_ *= 2;
        }
    }
}

void DiskCreator::Draw(vamiga::VAmiga& emu) {
    if (open_) {
        ImGui::OpenPopup("Create Disk");
        open_ = false;
    }

    if (ImGui::BeginPopupModal("Create Disk", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        std::string title = is_hard_disk_ ? std::format("Hard Disk (HD{})", target_drive_nr_) 
                                          : std::format("Floppy Disk (DF{})", target_drive_nr_);
        ImGui::Text("Creating %s", title.c_str());
        ImGui::Separator();

        if (!is_hard_disk_) {
            ImGui::Combo("Type", &floppy_type_, "3.5\" DD (880KB)\03.5\" HD (1.76MB)\0");
            ImGui::Combo("File System", &floppy_fs_, "OFS\0FFS\0NODOS\0");
            if (floppy_fs_ != 2) {
                ImGui::Combo("Boot Block", &floppy_boot_, "None\0AmigaDOS 1.3\0AmigaDOS 2.0\0");
                if (floppy_label_.empty()) floppy_label_.assign("Empty");
                ImGui::InputText("Label", &floppy_label_);
            }
        } else {
            if (ImGui::Combo("Capacity", &hd_capacity_idx_, "10 MB\020 MB\040 MB\080 MB\0Custom\0")) {
                UpdateHDCapacity();
            }
            
            bool custom = (hd_capacity_idx_ == 4);
            if (!custom) ImGui::BeginDisabled();
            ImGui::InputInt("Cylinders", &hd_c_);
            ImGui::InputInt("Heads", &hd_h_);
            ImGui::InputInt("Sectors", &hd_s_);
            if (!custom) ImGui::EndDisabled();
            
            ImGui::Combo("File System", &hd_fs_, "NODOS\0OFS\0FFS\0");
            if (hd_fs_ != 0) {
                if (hd_label_.empty()) hd_label_.assign("System");
                ImGui::InputText("Label", &hd_label_);
            }
        }
        
        ImGui::Separator();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(is_hard_disk_ ? "Create & Attach" : "Create & Insert")) {
            if (is_hard_disk_) CreateHardDisk(emu);
            else CreateFloppy(emu);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DiskCreator::CreateFloppy(vamiga::VAmiga& emu) {
    vamiga::FSFormat fs;
    switch(floppy_fs_) {
        case 0: fs = vamiga::FSFormat::OFS; break;
        case 1: fs = vamiga::FSFormat::FFS; break;
        default: fs = vamiga::FSFormat::NODOS; break;
    }
    
    vamiga::BootBlockId bb = vamiga::BootBlockId::NONE;
    if (floppy_fs_ != 2) {
        switch(floppy_boot_) {
            case 1: bb = vamiga::BootBlockId::AMIGADOS_13; break;
            case 2: bb = vamiga::BootBlockId::AMIGADOS_20; break;
        }
    }
    
    emu.df[target_drive_nr_]->insertBlankDisk(fs, bb, floppy_label_.c_str());
}

void DiskCreator::CreateHardDisk(vamiga::VAmiga& emu) {
    try {
        emu.hd[target_drive_nr_]->attach(hd_c_, hd_h_, hd_s_, 512);
        
        vamiga::FSFormat fs = vamiga::FSFormat::NODOS;
        if (hd_fs_ == 1) fs = vamiga::FSFormat::OFS;
        if (hd_fs_ == 2) fs = vamiga::FSFormat::FFS;
        
        emu.hd[target_drive_nr_]->format(fs, hd_label_.c_str());
        
    } catch (...) {
    }
}

}
