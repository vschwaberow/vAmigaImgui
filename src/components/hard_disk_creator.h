#ifndef COMPONENT_HARD_DISK_CREATOR_H
#define COMPONENT_HARD_DISK_CREATOR_H

#include <string>
#include <filesystem>

namespace gui {

class HardDiskCreator {
public:
  struct Geometry {
    int cylinders = 0;
    int heads = 0;
    int sectors = 0;
    int block_size = 512;
  };

  static HardDiskCreator& Instance();
  static Geometry CalculateGeometryForSize(int size_mb);

  void Open();
  void Draw();

  bool CreateHDF(const std::filesystem::path& path);

private:
  HardDiskCreator();

  bool open_ = false;
  int selected_preset_mb_ = 10;
  Geometry current_geometry_;
  
  bool custom_geometry_ = false;

  std::string file_path_;
};

}

#endif
