#include "hard_disk_creator.h"
#include "file_picker.h"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <vector>

namespace gui {

HardDiskCreator& HardDiskCreator::Instance() {
  static HardDiskCreator instance;
  return instance;
}

HardDiskCreator::HardDiskCreator() {
  current_geometry_ = CalculateGeometryForSize(selected_preset_mb_);
}

void HardDiskCreator::Open() {
  open_ = true;
  current_geometry_ = CalculateGeometryForSize(selected_preset_mb_);
}

HardDiskCreator::Geometry HardDiskCreator::CalculateGeometryForSize(int size_mb) {
    Geometry geom;
    if (size_mb <= 0) return geom;

    geom.block_size = 512;
    geom.sectors = 32;
    geom.heads = 1;
    
    long long total_bytes = static_cast<long long>(size_mb) * 1024 * 1024;
    long long track_size = static_cast<long long>(geom.heads) * geom.sectors * geom.block_size;
    
    if (track_size == 0) return geom;

    geom.cylinders = static_cast<int>(total_bytes / track_size);

    while (geom.cylinders > 1024) {
        geom.cylinders /= 2;
        geom.heads *= 2;
    }
    
    return geom;
}

void HardDiskCreator::Draw() {
    if (!open_) return;

    if (ImGui::Begin("Create Hard Disk", &open_)) {
        ImGui::Text("Size Preset:");
        
        if (ImGui::Button("10 MB")) { selected_preset_mb_ = 10; current_geometry_ = CalculateGeometryForSize(10); custom_geometry_ = false; } ImGui::SameLine();
        if (ImGui::Button("20 MB")) { selected_preset_mb_ = 20; current_geometry_ = CalculateGeometryForSize(20); custom_geometry_ = false; } ImGui::SameLine();
        if (ImGui::Button("40 MB")) { selected_preset_mb_ = 40; current_geometry_ = CalculateGeometryForSize(40); custom_geometry_ = false; } ImGui::SameLine();
        if (ImGui::Button("80 MB")) { selected_preset_mb_ = 80; current_geometry_ = CalculateGeometryForSize(80); custom_geometry_ = false; }

        ImGui::Separator();

        ImGui::Checkbox("Custom Geometry", &custom_geometry_);

        if (custom_geometry_) {
            ImGui::InputInt("Cylinders", &current_geometry_.cylinders);
            ImGui::InputInt("Heads", &current_geometry_.heads);
            ImGui::InputInt("Sectors", &current_geometry_.sectors);
        } else {
            ImGui::Text("Geometry: %d Cylinders, %d Heads, %d Sectors", current_geometry_.cylinders, current_geometry_.heads, current_geometry_.sectors);
            ImGui::Text("Block Size: %d", current_geometry_.block_size);
            long long size_bytes = (long long)current_geometry_.cylinders * current_geometry_.heads * current_geometry_.sectors * current_geometry_.block_size;
            ImGui::Text("Total Size: %.2f MB", size_bytes / (1024.0 * 1024.0));
        }

        ImGui::Separator();

        if (ImGui::Button("Create")) {
             PickerOptions opts;
             opts.title = "Create Hard Disk File";
             opts.filters = ".hdf";
             opts.mode = PickerMode::kSaveFile;
             
             FilePicker::Instance().Open("CreateHDF", opts, [this](std::filesystem::path path) {
                 if (CreateHDF(path)) {
                     open_ = false;
                 }
             });
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            open_ = false;
        }
    }
    ImGui::End();
}

bool HardDiskCreator::CreateHDF(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    long long size_bytes = static_cast<long long>(current_geometry_.cylinders) * 
                           current_geometry_.heads * 
                           current_geometry_.sectors * 
                           current_geometry_.block_size;

    const size_t chunk_size = 1024 * 1024;
    std::vector<char> buffer(chunk_size, 0);
    
    long long bytes_remaining = size_bytes;
    while (bytes_remaining > 0) {
        long long to_write = std::min(bytes_remaining, static_cast<long long>(chunk_size));
        file.write(buffer.data(), to_write);
        if (!file) return false;
        bytes_remaining -= to_write;
    }
    
    return true;
}

}
