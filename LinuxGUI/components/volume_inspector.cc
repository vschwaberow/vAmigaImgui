#include "volume_inspector.h"

#include <algorithm>
#include <array>
#include <format>
#include <ranges>
#include <string_view>

#include "FileSystems/FSBlock.h"
#include "FileSystems/FSTypes.h"
#include "resources/IconsFontAwesome6.h"

namespace gui {

namespace {
template <typename Enum, std::size_t N>
requires std::is_enum_v<Enum>
constexpr ImVec4 EnumColor(Enum e, const std::array<ImVec4, N>& map,
                           ImVec4 fallback = ImVec4(0.75f, 0.75f, 0.75f, 1.0f)) {
  auto idx = static_cast<std::size_t>(std::to_underlying(e));
  if (idx < map.size()) return map[idx];
  return fallback;
}

template <typename Enum, std::size_t N, typename CountAccessor>
requires std::is_enum_v<Enum>
void DrawLegend(const char* table_id, const std::array<ImVec4, N>& colors,
                std::span<const Enum> order, CountAccessor&& count_fn) {
  if (ImGui::BeginTable(table_id, 2, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 24.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
    for (auto type : order) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::ColorButton(
          "##col", EnumColor(type, colors),
          ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
          ImVec2(18, 18));
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s (%d)", vamiga::FSBlockTypeEnum::_key(type),
                  count_fn(type));
    }
    ImGui::EndTable();
  }
}

std::string_view BootBlockName(vamiga::BootBlockType type) {
  switch (type) {
    case vamiga::BootBlockType::STANDARD: return "Standard";
    case vamiga::BootBlockType::VIRUS: return "Virus";
    case vamiga::BootBlockType::CUSTOM: return "Custom";
  }
  return "Unknown";
}

enum class AllocationState : uint8_t { Free = 0, Allocated = 1, Ambiguous = 2, Conflict = 3 };
enum class HealthState : uint8_t { Unused = 0, Ok = 1, Error = 2 };

constexpr std::array<ImVec4, 11> kFsColors = {
    ImVec4(1.0f, 0.70f, 0.4f, 1.0f),   // BOOT
    ImVec4(1.0f, 0.40f, 0.40f, 1.0f),  // ROOT
    ImVec4(0.74f, 0.52f, 1.0f, 1.0f),  // BITMAP
    ImVec4(0.98f, 0.40f, 0.98f, 1.0f), // BITMAP_EXT
    ImVec4(1.0f, 0.94f, 0.52f, 1.0f),  // USERDIR
    ImVec4(0.40f, 0.66f, 1.0f, 1.0f),  // FILEHEADER
    ImVec4(0.0f, 0.60f, 0.0f, 1.0f),   // FILELIST
    ImVec4(0.52f, 0.93f, 0.52f, 1.0f), // DATA_OFS
    ImVec4(0.52f, 0.93f, 0.52f, 1.0f), // DATA_FFS
    ImVec4(0.60f, 0.60f, 0.60f, 1.0f), // EMPTY
    ImVec4(0.85f, 0.85f, 0.85f, 1.0f)}; // UNKNOWN

constexpr std::array<ImVec4, 4> kAllocColors = {
    ImVec4(0.60f, 0.60f, 0.60f, 1.0f),
    ImVec4(0.40f, 0.80f, 0.40f, 1.0f),
    ImVec4(0.95f, 0.85f, 0.40f, 1.0f),
    ImVec4(0.90f, 0.25f, 0.25f, 1.0f),
};

constexpr std::array<ImVec4, 3> kHealthColors = {
    ImVec4(0.60f, 0.60f, 0.60f, 1.0f),
    ImVec4(0.40f, 0.80f, 0.40f, 1.0f),
    ImVec4(0.90f, 0.25f, 0.25f, 1.0f),
};
}  // namespace

VolumeInspector& VolumeInspector::Instance() {
  static VolumeInspector instance;
  return instance;
}

void VolumeInspector::Open(int drive_nr, bool is_hd, vamiga::VAmiga& emu, int partition) {
  drive_nr_ = drive_nr;
  is_hd_ = is_hd;
  part_ = partition;
  Refresh(emu);
  open_ = true;
}

void VolumeInspector::Refresh(vamiga::VAmiga& emu) {
  fs_.reset();
  usage_map_.clear();
  alloc_map_.clear();
  health_map_.clear();
  type_counts_.fill(0);

  try {
    if (is_hd_) {
      auto hd_info = emu.hd[drive_nr_]->getInfo();
      if (hd_info.hasDisk && emu.hd[drive_nr_]->drive) {
        fs_ = std::make_unique<vamiga::MutableFileSystem>(*emu.hd[drive_nr_]->drive,
                                                          part_);
      }
    } else {
      auto df_info = emu.df[drive_nr_]->getInfo();
      if (df_info.hasDisk && emu.df[drive_nr_]->drive) {
        fs_ = std::make_unique<vamiga::MutableFileSystem>(*emu.df[drive_nr_]->drive);
      }
    }
  } catch (...) {
    fs_.reset();
  }

  if (fs_) {
    info_ = fs_->getInfo();
    const size_t block_count = static_cast<size_t>(info_.numBlocks);
    usage_map_.resize(block_count);
    alloc_map_.resize(block_count);
    health_map_.resize(block_count);
    if (!usage_map_.empty()) {
      fs_->createUsageMap(usage_map_.data(), static_cast<vamiga::isize>(usage_map_.size()));
      fs_->createAllocationMap(alloc_map_.data(), static_cast<vamiga::isize>(alloc_map_.size()));
      fs_->createHealthMap(health_map_.data(), static_cast<vamiga::isize>(health_map_.size()));
      for (auto v : usage_map_) {
        auto idx = static_cast<size_t>(v);
        if (idx < type_counts_.size()) type_counts_[idx]++;
      }
    }
    selected_block_ = std::clamp(selected_block_, 0, static_cast<int>(info_.numBlocks > 0 ? info_.numBlocks - 1 : 0));
  }
}

void VolumeInspector::Draw(vamiga::VAmiga& emu) {
  if (open_) {
    ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
    ImGui::OpenPopup("Volume Inspector");
    open_ = false;
  }

  if (ImGui::BeginPopupModal("Volume Inspector", nullptr, ImGuiWindowFlags_None)) {
    int partitions = 1;
    if (is_hd_) {
      auto hd_info = emu.hd[drive_nr_]->getInfo();
      partitions = std::max(1, static_cast<int>(hd_info.partitions));
      part_ = std::clamp(part_, 0, partitions - 1);
    }

    if (!fs_) {
      ImGui::Text("No formatted volume available.");
      if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    if (ImGui::Button(ICON_FA_ROTATE " Refresh")) {
      Refresh(emu);
    }
    ImGui::SameLine();
    ImGui::Text("%s %s%d", is_hd_ ? "Hard Disk" : "Floppy", is_hd_ ? "HD" : "DF",
                drive_nr_);
    if (is_hd_ && partitions > 1) {
      ImGui::SameLine();
      int part = part_;
      ImGui::SetNextItemWidth(120.0f);
      if (ImGui::SliderInt("Partition", &part, 0, partitions - 1, "Part %d")) {
        part_ = std::clamp(part, 0, partitions - 1);
        Refresh(emu);
      }
    }

    DrawInfo();
    ImGui::Separator();
    DrawUsage();
    ImGui::Separator();
    DrawAllocation();
    ImGui::Separator();
    DrawHealth(emu);
    ImGui::Separator();
    DrawBlockView();

    ImGui::Separator();
    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void VolumeInspector::DrawInfo() {
  auto traits = fs_->getTraits();
  auto row = [](std::string_view key, auto&& value) {
    std::string val = std::format("{}", std::forward<decltype(value)>(value));
    ImGui::Text("%.*s%s", static_cast<int>(key.size()), key.data(), val.c_str());
  };
  row("Name: ", info_.name);
  row("Created: ", info_.creationDate);
  row("Modified: ", info_.modificationDate);

  ImGui::Text("Format: %s (%s)",
              vamiga::FSFormatEnum::_key(traits.dos),
              vamiga::isOFSVolumeType(traits.dos) ? "OFS" : "FFS");
  ImGui::SameLine();
  ImGui::Text("Boot: %s", BootBlockName(fs_->bootBlockType()).data());
  ImGui::Text("Blocks: %lld  Free: %lld  Used: %lld",
              static_cast<long long>(info_.numBlocks),
              static_cast<long long>(info_.freeBlocks),
              static_cast<long long>(info_.usedBlocks));
  ImGui::Text("Bytes: used %.2f KB / total %.2f KB",
              info_.usedBytes / 1024.0, info_.numBlocks * traits.bsize / 1024.0);
}

void VolumeInspector::DrawUsage() {
  if (usage_map_.empty()) return;

  ImGui::Text("Usage Map");
  const float cell = 10.0f;
  int cols = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
  auto draw_list = ImGui::GetWindowDrawList();
  ImVec2 start = ImGui::GetCursorScreenPos();

  for (int i = 0; i < static_cast<int>(usage_map_.size()); ++i) {
    int row = i / cols;
    int col = i % cols;
    ImVec2 p0 = ImVec2(start.x + col * cell, start.y + row * cell);
    ImVec2 p1 = ImVec2(p0.x + cell - 1, p0.y + cell - 1);
    auto type = static_cast<vamiga::FSBlockType>(usage_map_[static_cast<size_t>(i)]);
    draw_list->AddRectFilled(p0, p1,
                             ImGui::GetColorU32(EnumColor(type, kFsColors)));
    if (ImGui::IsMouseHoveringRect(p0, p1) && ImGui::IsWindowHovered()) {
      ImGui::SetTooltip("Block %d: %s", i, vamiga::FSBlockTypeEnum::_key(type));
      if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        selected_block_ = i;
      }
    }
  }
  int rows = (static_cast<int>(usage_map_.size()) + cols - 1) / cols;
  ImGui::Dummy(ImVec2(0, rows * cell));

  constexpr std::array usage_order = {
      vamiga::FSBlockType::BOOT,     vamiga::FSBlockType::ROOT,
      vamiga::FSBlockType::BITMAP,   vamiga::FSBlockType::BITMAP_EXT,
      vamiga::FSBlockType::USERDIR,  vamiga::FSBlockType::FILEHEADER,
      vamiga::FSBlockType::FILELIST, vamiga::FSBlockType::DATA_OFS,
      vamiga::FSBlockType::EMPTY,    vamiga::FSBlockType::UNKNOWN};
  DrawLegend("UsageLegend", kFsColors, std::span<const vamiga::FSBlockType>(usage_order),
             [&](vamiga::FSBlockType t) {
               return type_counts_[static_cast<size_t>(t)];
             });
}

void VolumeInspector::DrawAllocation() {
  if (alloc_map_.empty()) return;

  ImGui::Text("Allocation Map");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_WRENCH " Rectify")) {
    if (fs_) {
      fs_->rectifyAllocationMap();
      fs_->createAllocationMap(alloc_map_.data(),
                               static_cast<vamiga::isize>(alloc_map_.size()));
    }
  }

  const float cell = 10.0f;
  int cols = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
  auto draw_list = ImGui::GetWindowDrawList();
  ImVec2 start = ImGui::GetCursorScreenPos();

  for (int i = 0; i < static_cast<int>(alloc_map_.size()); ++i) {
    int row = i / cols;
    int col = i % cols;
    ImVec2 p0 = ImVec2(start.x + col * cell, start.y + row * cell);
    ImVec2 p1 = ImVec2(p0.x + cell - 1, p0.y + cell - 1);
    auto val = static_cast<AllocationState>(alloc_map_[static_cast<size_t>(i)]);
    draw_list->AddRectFilled(p0, p1,
                             ImGui::GetColorU32(EnumColor(val, kAllocColors)));
  }
  int rows = (static_cast<int>(alloc_map_.size()) + cols - 1) / cols;
  ImGui::Dummy(ImVec2(0, rows * cell));

  if (ImGui::BeginTable("AllocLegend", 2, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 24.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch);
    struct Legend { std::string_view label; AllocationState code; };
    constexpr std::array legend = {
        Legend{"Free", AllocationState::Free},
        Legend{"Allocated", AllocationState::Allocated},
        Legend{"Ambiguous", AllocationState::Ambiguous},
        Legend{"Conflict", AllocationState::Conflict}};
    for (const auto& entry : legend) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::ColorButton("##col", EnumColor(entry.code, kAllocColors),
                         ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                         ImVec2(18, 18));
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", entry.label.data());
    }
    ImGui::EndTable();
  }
}

void VolumeInspector::DrawHealth(vamiga::VAmiga& emu) {
  if (health_map_.empty()) return;

  ImGui::Text("Health Map");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_ROTATE " Re-run Check")) {
    if (fs_) {
      fs_->createHealthMap(health_map_.data(),
                           static_cast<vamiga::isize>(health_map_.size()));
    }
  }

  const float cell = 10.0f;
  int cols = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
  auto draw_list = ImGui::GetWindowDrawList();
  ImVec2 start = ImGui::GetCursorScreenPos();

  for (int i = 0; i < static_cast<int>(health_map_.size()); ++i) {
    int row = i / cols;
    int col = i % cols;
    ImVec2 p0 = ImVec2(start.x + col * cell, start.y + row * cell);
    ImVec2 p1 = ImVec2(p0.x + cell - 1, p0.y + cell - 1);
    auto val = static_cast<HealthState>(health_map_[static_cast<size_t>(i)]);
    draw_list->AddRectFilled(p0, p1,
                             ImGui::GetColorU32(EnumColor(val, kHealthColors)));
  }
  int rows = (static_cast<int>(health_map_.size()) + cols - 1) / cols;
  ImGui::Dummy(ImVec2(0, rows * cell));

  if (ImGui::BeginTable("HealthLegend", 2, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 24.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch);
    struct Legend { std::string_view label; HealthState code; };
    constexpr std::array legend = {
        Legend{"Unused", HealthState::Unused},
        Legend{"OK", HealthState::Ok},
        Legend{"Error", HealthState::Error}};
    for (const auto& entry : legend) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::ColorButton("##col", EnumColor(entry.code, kHealthColors),
                         ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                         ImVec2(18, 18));
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", entry.label.data());
    }
    ImGui::EndTable();
  }
}

void VolumeInspector::DrawBlockView() {
  auto traits = fs_->getTraits();
  selected_block_ = std::clamp(selected_block_, 0,
                               static_cast<int>(info_.numBlocks > 0 ? info_.numBlocks - 1 : 0));
  ImGui::Text("Block %d", selected_block_);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(180.0f);
  if (ImGui::SliderInt("##blk", &selected_block_, 0,
                       static_cast<int>(std::max<vamiga::isize>(info_.numBlocks - 1, 0)))) {
  }

  auto* blk = fs_->read(static_cast<vamiga::Block>(selected_block_));
  if (!blk) {
    ImGui::TextDisabled("Unable to read block.");
    return;
  }

  const uint8_t* data = blk->data();
  const int size = static_cast<int>(traits.bsize);

  if (ImGui::BeginTable("BlockHex", 18,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 60);
    for (int i : std::views::iota(0, 16)) {
      ImGui::TableSetupColumn(std::format("{:02X}", i).c_str(),
                              ImGuiTableColumnFlags_WidthFixed, 25);
    }
    ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, 120);

    for (int row : std::views::iota(0, size / 16)) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%04X", row * 16);
      for (int col : std::views::iota(0, 16)) {
        int idx = row * 16 + col;
        ImGui::TableSetColumnIndex(col + 1);
        ImGui::Text("%02X", data[idx]);
      }
      ImGui::TableSetColumnIndex(17);
      std::array<char, 17> ascii{};
      for (int col = 0; col < 16; ++col) {
        uint8_t v = data[row * 16 + col];
        ascii[static_cast<size_t>(col)] =
            (v >= 32 && v < 127) ? static_cast<char>(v) : '.';
      }
      ImGui::Text("%s", ascii.data());
    }
    ImGui::EndTable();
  }
}

}  // namespace gui
