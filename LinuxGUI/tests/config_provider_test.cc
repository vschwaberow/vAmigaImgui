#include <gtest/gtest.h>
#include <string>

#include "services/config_provider.h"
#include "constants.h"

TEST(ConfigProviderTest, UsesFallbacksWhenUnset) {
  vamiga::Defaults defaults;
  auto api = std::make_unique<vamiga::DefaultsAPI>(&defaults);
  gui::ConfigProvider provider(*api);

  EXPECT_EQ(provider.GetInt(gui::ConfigKeys::kAudioVolume, -1),
            gui::Defaults::kAudioVolume);
  EXPECT_TRUE(provider.GetBool(gui::ConfigKeys::kUiVideoBack, false));
  EXPECT_EQ(provider.GetString(gui::ConfigKeys::kKickstartPath), "");
}

TEST(ConfigProviderTest, SettersOverrideValues) {
  vamiga::Defaults defaults;
  auto api = std::make_unique<vamiga::DefaultsAPI>(&defaults);
  gui::ConfigProvider provider(*api);

  provider.SetBool(gui::ConfigKeys::kUiVideoBack, false);
  EXPECT_FALSE(provider.GetBool(gui::ConfigKeys::kUiVideoBack, true));

  provider.SetInt(gui::ConfigKeys::kAudioVolume, 42);
  EXPECT_EQ(provider.GetInt(gui::ConfigKeys::kAudioVolume, 0), 42);

  provider.SetString(gui::ConfigKeys::kKickstartPath, "/tmp/kick.rom");
  EXPECT_EQ(provider.GetString(gui::ConfigKeys::kKickstartPath), "/tmp/kick.rom");

  provider.SetFloppyPath(1, "disk1.adf");
  EXPECT_EQ(provider.GetFloppyPath(1), "disk1.adf");
}

TEST(ConfigProviderTest, GetIntFallsBackOnInvalid) {
  vamiga::Defaults defaults;
  auto api = std::make_unique<vamiga::DefaultsAPI>(&defaults);
  gui::ConfigProvider provider(*api);

  provider.SetString(gui::ConfigKeys::kAudioVolume, "not-an-int");
  EXPECT_EQ(provider.GetInt(gui::ConfigKeys::kAudioVolume, 7), 7);
}
