#include "disk_inspector.h"
#include <algorithm>
#include <format>
#include <cmath>
#include <ranges>
#include "resources/IconsFontAwesome6.h"

namespace gui {

DiskInspector& DiskInspector::Instance() {
    static DiskInspector instance;
    return instance;
}

void DiskInspector::Open(int drive_nr, bool is_hd, vamiga::VAmiga& emu) {
    drive_nr_ = drive_nr;
    is_hd_ = is_hd;
    UpdateMedia(emu);
    open_ = true;
}

void DiskInspector::UpdateMedia(vamiga::VAmiga& emu) {
    media_.reset();
    
    if (is_hd_) {
        media_.reset(vamiga::MediaFile::make(*emu.hd[drive_nr_], vamiga::FileType::HDF));
    } else {
        media_.reset(vamiga::MediaFile::make(*emu.df[drive_nr_], vamiga::FileType::ADF));
        if (!media_) media_.reset(vamiga::MediaFile::make(*emu.df[drive_nr_], vamiga::FileType::IMG));
        if (!media_) media_.reset(vamiga::MediaFile::make(*emu.df[drive_nr_], vamiga::FileType::EADF));
    }
    
    if (media_) {
        auto info = media_->getDiskInfo();
        num_cyls_ = static_cast<int>(info.cyls);
        num_heads_ = static_cast<int>(info.heads);
        num_sectors_ = static_cast<int>(info.sectors);
        num_blocks_ = static_cast<int>(info.blocks);
        num_tracks_ = static_cast<int>(info.tracks);
    } else {
        num_cyls_ = 0; num_heads_ = 0; num_sectors_ = 0; num_blocks_ = 0; num_tracks_ = 0;
    }
    
    current_block_ = 0;
    UpdateSelectionFromBlock();
}

void DiskInspector::UpdateSelectionFromBlock() {
    if (num_sectors_ == 0 || num_heads_ == 0) return;
    
    current_block_ = std::clamp(current_block_, 0, num_blocks_ - 1);
    
    current_track_ = current_block_ / num_sectors_;
    current_sector_ = current_block_ % num_sectors_;
    current_cyl_ = current_track_ / num_heads_;
    current_head_ = current_track_ % num_heads_;
}

void DiskInspector::UpdateSelectionFromGeometry() {
    if (num_sectors_ == 0 || num_heads_ == 0) return;

    current_cyl_ = std::clamp(current_cyl_, 0, num_cyls_ - 1);
    current_head_ = std::clamp(current_head_, 0, num_heads_ - 1);
    current_sector_ = std::clamp(current_sector_, 0, num_sectors_ - 1);
    
    current_track_ = current_cyl_ * num_heads_ + current_head_;
    current_block_ = current_track_ * num_sectors_ + current_sector_;
}

void DiskInspector::Draw(vamiga::VAmiga& emu) {
    if (open_) {
        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        ImGui::OpenPopup("Disk Inspector");
        open_ = false;
    }

    if (ImGui::BeginPopupModal("Disk Inspector", nullptr, ImGuiWindowFlags_None)) {
        
        if (!media_) {
            ImGui::Text("No media inserted or attached.");
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        
        DrawInfo();
        ImGui::Separator();
        DrawNavigation();
        ImGui::Separator();
        
        if (ImGui::BeginTabBar("InspectorTabs")) {
            if (ImGui::BeginTabItem("Block View")) {
                DrawBlockView();
                ImGui::EndTabItem();
            }
            if (!is_hd_ && ImGui::BeginTabItem("MFM View")) {
                DrawMFMView(emu);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        
        ImGui::Separator();
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        
        ImGui::EndPopup();
    }
}

void DiskInspector::DrawInfo() {
    std::string type = is_hd_ ? "Hard Disk" : "Floppy Disk";
    ImGui::Text("%s %s%d", type.c_str(), is_hd_ ? "HD" : "DF", drive_nr_);
    
    if (ImGui::BeginTable("InfoTable", 4)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("Cylinders: %d", num_cyls_);
        ImGui::TableSetColumnIndex(1); ImGui::Text("Heads: %d", num_heads_);
        ImGui::TableSetColumnIndex(2); ImGui::Text("Sectors: %d", num_sectors_);
        ImGui::TableSetColumnIndex(3); ImGui::Text("Blocks: %d", num_blocks_);
        ImGui::EndTable();
    }
    
    auto info = media_->getDiskInfo();
    ImGui::Text("Block Size: %ld", (long)info.bsize);
    ImGui::SameLine();
    ImGui::Text("Capacity: %s", media_->getSizeAsString().c_str());
}

void DiskInspector::DrawNavigation() {
    if (ImGui::BeginTable("NavTable", 6)) {
        ImGui::TableSetupColumn("Label");
        ImGui::TableSetupColumn("Input");
        ImGui::TableSetupColumn("Step");
        ImGui::TableSetupColumn("Label2");
        ImGui::TableSetupColumn("Input2");
        ImGui::TableSetupColumn("Step2");
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("Cylinder");
        ImGui::TableSetColumnIndex(1); 
        if (ImGui::InputInt("##cyl", &current_cyl_)) UpdateSelectionFromGeometry();
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("-##cyl")) { current_cyl_--; UpdateSelectionFromGeometry(); }
        ImGui::SameLine();
        if (ImGui::Button("+##cyl")) { current_cyl_++; UpdateSelectionFromGeometry(); }
        
        ImGui::TableSetColumnIndex(3); ImGui::Text("Head");
        ImGui::TableSetColumnIndex(4);
        if (ImGui::InputInt("##head", &current_head_)) UpdateSelectionFromGeometry();
        ImGui::TableSetColumnIndex(5);
        if (ImGui::Button("-##head")) { current_head_--; UpdateSelectionFromGeometry(); }
        ImGui::SameLine();
        if (ImGui::Button("+##head")) { current_head_++; UpdateSelectionFromGeometry(); }
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("Sector");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::InputInt("##sec", &current_sector_)) UpdateSelectionFromGeometry();
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("-##sec")) { current_sector_--; UpdateSelectionFromGeometry(); }
        ImGui::SameLine();
        if (ImGui::Button("+##sec")) { current_sector_++; UpdateSelectionFromGeometry(); }
        
        ImGui::TableSetColumnIndex(3); ImGui::Text("Block");
        ImGui::TableSetColumnIndex(4);
        if (ImGui::InputInt("##blk", &current_block_)) UpdateSelectionFromBlock();
        ImGui::TableSetColumnIndex(5);
        if (ImGui::Button("-##blk")) { current_block_--; UpdateSelectionFromBlock(); }
        ImGui::SameLine();
        if (ImGui::Button("+##blk")) { current_block_++; UpdateSelectionFromBlock(); }
        
        ImGui::EndTable();
    }
}

void DiskInspector::DrawBlockView() {
    if (ImGui::BeginTable("HexView", 18, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 60);
        for (int i : std::views::iota(0, 16)) {
            ImGui::TableSetupColumn(std::format("{:02X}", i).c_str(), ImGuiTableColumnFlags_WidthFixed, 25);
        }
        ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        for (int r : std::views::iota(0, 32)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%03X", r * 16);
            
            for (int c : std::views::iota(0, 16)) {
                ImGui::TableSetColumnIndex(c + 1);
                uint8_t byte = media_->readByte(current_block_, r * 16 + c);
                ImGui::Text("%02X", byte);
            }
            
            ImGui::TableSetColumnIndex(17);
            std::string ascii = media_->asciidump(current_block_, r * 16, 16);
            ImGui::Text("%s", ascii.c_str());
        }
        ImGui::EndTable();
    }
}

void DiskInspector::DrawMFMView(vamiga::VAmiga& emu) {
    ImGui::Text("MFM Track Data (Track %d)", current_track_);
    ImGui::BeginChild("MFMScroll", ImVec2(0, 300), true);
    
    std::string mfm = emu.df[drive_nr_]->readTrackBits(current_track_);
    if (mfm.empty()) {
        ImGui::TextDisabled("No MFM data available");
    } else {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); 
        
        ImGui::TextWrapped("%s", mfm.c_str());
        
        ImGui::PopFont();
    }
    ImGui::EndChild();
}

}
