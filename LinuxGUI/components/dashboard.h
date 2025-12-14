#ifndef LINUXGUI_COMPONENTS_DASHBOARD_H_
#define LINUXGUI_COMPONENTS_DASHBOARD_H_

#include <string_view>

#include <vector>

#include <utility>
#include "VAmiga.h"
#undef unreachable
#define unreachable std::unreachable()

namespace gui {

class Dashboard {

 public:

  static Dashboard& Instance();

    void Draw(bool* p_open, vamiga::VAmiga& emu);

   private:

    Dashboard();

    void UpdateData(vamiga::VAmiga& emu);

    void DrawPlot(std::string_view label, const std::vector<float>& data, float min,

                  float max, std::string_view overlay_text = "");

    static constexpr int kHistorySize = 100;

    std::vector<float> cpu_load_;

    std::vector<float> gpu_fps_;

    std::vector<float> emu_fps_;

    std::vector<float> chip_ram_activity_;

    std::vector<float> slow_ram_activity_;

    std::vector<float> fast_ram_activity_;

    std::vector<float> audio_buffer_fill_;

    std::vector<float> audio_waveform_buffer_;

  };

  }

#endif   
