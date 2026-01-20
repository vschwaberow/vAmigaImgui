#include "components/hard_disk_creator.h"
#include <fstream>
#include <gtest/gtest.h>

TEST(HardDiskCreatorTest, CalculateGeometry_10MB) {
  auto geom = gui::HardDiskCreator::CalculateGeometryForSize(10);

  EXPECT_EQ(geom.cylinders, 640);
  EXPECT_EQ(geom.heads, 1);
  EXPECT_EQ(geom.sectors, 32);
  EXPECT_EQ(geom.block_size, 512);
}

TEST(HardDiskCreatorTest, CalculateGeometry_40MB) {
  auto geom = gui::HardDiskCreator::CalculateGeometryForSize(40);

  EXPECT_EQ(geom.cylinders, 640);
  EXPECT_EQ(geom.heads, 4);
  EXPECT_EQ(geom.sectors, 32);
  EXPECT_EQ(geom.block_size, 512);
}

TEST(HardDiskCreatorTest, CalculateGeometry_Zero) {
  auto geom = gui::HardDiskCreator::CalculateGeometryForSize(0);
  EXPECT_EQ(geom.cylinders, 0);
}

TEST(HardDiskCreatorTest, CreateHDF) {
    auto& creator = gui::HardDiskCreator::Instance();
    std::string temp_path = "test_disk.hdf";
    
    EXPECT_TRUE(creator.CreateHDF(temp_path));
    
    std::ifstream file(temp_path, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(file.good());
    std::streamsize size = file.tellg();
    
    long long expected_size = (long long)640 * 1 * 32 * 512;
    EXPECT_EQ(size, expected_size);
    
    std::remove(temp_path.c_str());
}
