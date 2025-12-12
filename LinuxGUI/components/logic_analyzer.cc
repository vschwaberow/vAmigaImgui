#include "logic_analyzer.h"
#include <format>
#include <ranges>
#include "Misc/LogicAnalyzer/LogicAnalyzerTypes.h"
#include "Infrastructure/OptionTypes.h"
#include "resources/IconsFontAwesome6.h"

namespace gui {

const std::vector<LogicAnalyzer::ProbePreset> LogicAnalyzer::kPresets = {
    {"None", 0, 0},
    {"-", 0, 0},
    {"DMACONR", 1, 0xDFF002},
    {"VPOSR", 1, 0xDFF004},
    {"VHPOSR", 1, 0xDFF006},
    {"DSKDATR", 1, 0xDFF008},
    {"JOY0DAT", 1, 0xDFF00A},
    {"JOY1DAT", 1, 0xDFF00C},
    {"CLXDAT", 1, 0xDFF00E},
    {"ADKCONR", 1, 0xDFF010},
    {"POT0DAT", 1, 0xDFF012},
    {"POT1DAT", 1, 0xDFF014},
    {"POTGOR", 1, 0xDFF016},
    {"SERDATR", 1, 0xDFF018},
    {"DSKBYTR", 1, 0xDFF01A},
    {"INTENAR", 1, 0xDFF01C},
    {"INTREQR", 1, 0xDFF01E},
    {"DENISEID", 1, 0xDFF07C},
    {"-", 0, 0},
    {"IPL", 2, 0},
    {"-", 0, 0}};

LogicAnalyzer& LogicAnalyzer::Instance() {
  static LogicAnalyzer instance;
  return instance;
}

LogicAnalyzer::LogicAnalyzer() {}

void LogicAnalyzer::Update(vamiga::VAmiga& emu) {
  auto la_info = emu.agnus.logicAnalyzer.getInfo();
  if (!la_info.busOwner) return;

  long hpos = emu.agnus.getInfo().hpos;

  for (int i : std::views::iota(0, kSegments)) {
    labels_[i] = "";
    colors_[i] = 0;
    
    if (i < hpos) {
      vamiga::BusOwner owner = la_info.busOwner[i];
      switch (owner) {
        case vamiga::BusOwner::CPU:
          labels_[i] = "CPU";
          colors_[i] = IM_COL32(100, 150, 250, 255);
          break;
        case vamiga::BusOwner::REFRESH:
          labels_[i] = "REF";
          colors_[i] = IM_COL32(100, 100, 100, 255);
          break;
        case vamiga::BusOwner::DISK:
          labels_[i] = "DSK";
          colors_[i] = IM_COL32(200, 200, 50, 255);
          break;
        case vamiga::BusOwner::AUD0:
        case vamiga::BusOwner::AUD1:
        case vamiga::BusOwner::AUD2:
        case vamiga::BusOwner::AUD3:
          labels_[i] = std::format("AUD{}", static_cast<int>(owner) - static_cast<int>(vamiga::BusOwner::AUD0));
          colors_[i] = IM_COL32(200, 100, 50, 255);
          break;
        case vamiga::BusOwner::BPL1:
        case vamiga::BusOwner::BPL2:
        case vamiga::BusOwner::BPL3:
        case vamiga::BusOwner::BPL4:
        case vamiga::BusOwner::BPL5:
        case vamiga::BusOwner::BPL6:
          labels_[i] = std::format("BPL{}", static_cast<int>(owner) - static_cast<int>(vamiga::BusOwner::BPL1) + 1);
          colors_[i] = IM_COL32(100, 150, 255, 255);
          break;
        case vamiga::BusOwner::SPRITE0:
        case vamiga::BusOwner::SPRITE1:
        case vamiga::BusOwner::SPRITE2:
        case vamiga::BusOwner::SPRITE3:
        case vamiga::BusOwner::SPRITE4:
        case vamiga::BusOwner::SPRITE5:
        case vamiga::BusOwner::SPRITE6:
        case vamiga::BusOwner::SPRITE7:
          labels_[i] = std::format("SPR{}", static_cast<int>(owner) - static_cast<int>(vamiga::BusOwner::SPRITE0));
          colors_[i] = IM_COL32(200, 50, 200, 255);
          break;
        case vamiga::BusOwner::COPPER:
          labels_[i] = "COP";
          colors_[i] = IM_COL32(50, 200, 50, 255);
          break;
        case vamiga::BusOwner::BLITTER:
          labels_[i] = "BLT";
          colors_[i] = IM_COL32(50, 200, 200, 255);
          break;
        default:
          labels_[i] = "-";
          colors_[i] = 0;
          break;
      }

      data_[0][i] = la_info.addrBus[i];
      data_[1][i] = la_info.dataBus[i];
    } else {
        data_[0][i] = -1;
        data_[1][i] = -1;
    }
  }

  const vamiga::isize* channels[4] = {la_info.channel[0], la_info.channel[1],
                            la_info.channel[2], la_info.channel[3]};

  for (int c : std::views::iota(0, 4)) {
    if (channels[c]) {
      for (int i : std::views::iota(0, kSegments)) {
        data_[c + 2][i] = (i < hpos) ? static_cast<int>(channels[c][i]) : -1;
      }
    } else {
      for (int i : std::views::iota(0, kSegments)) {
        data_[c + 2][i] = -1;
      }
    }
  }
}

void LogicAnalyzer::UpdateProbe(vamiga::VAmiga& emu, int channel, int probe_type, uint32_t addr) {
    vamiga::Opt var;
    switch (channel) {
        case 0: var = vamiga::Opt::LA_PROBE0; break;
        case 1: var = vamiga::Opt::LA_PROBE1; break;
        case 2: var = vamiga::Opt::LA_PROBE2; break;
        case 3: var = vamiga::Opt::LA_PROBE3; break;
        default: return;
    }
    emu.set(var, probe_type);

    if (probe_type == 1) { 
        switch (channel) {
            case 0: var = vamiga::Opt::LA_ADDR0; break;
            case 1: var = vamiga::Opt::LA_ADDR1; break;
            case 2: var = vamiga::Opt::LA_ADDR2; break;
            case 3: var = vamiga::Opt::LA_ADDR3; break;
        }
        emu.set(var, static_cast<int>(addr));
    }
}


void LogicAnalyzer::DrawControls(vamiga::VAmiga& emu) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 0));

  ImGui::SetNextItemWidth(100);
  if (ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 20.0f, "%.1fx")) {
  }

  ImGui::SameLine();
  if (ImGui::Button(hex_mode_ ? "HEX" : "DEC")) hex_mode_ = !hex_mode_;
  ImGui::SetItemTooltip("Toggle Hex/Dec");

  ImGui::SameLine();
  if (ImGui::Button(symbolic_ ? "SYM" : "RAW")) symbolic_ = !symbolic_;
  ImGui::SetItemTooltip("Toggle Symbolic/Raw");

  for (int i : std::views::iota(0, 4)) {
      ImGui::SameLine();
      ImGui::PushID(i);
      ImGui::SetNextItemWidth(100);
      
      std::string_view preview = "Custom...";
      if (probe_types_[i] == 0) preview = "None";
      else if (probe_types_[i] == 2) preview = "IPL";
      else if (probe_types_[i] == 1) {
          bool found = false;
          for(const auto& p : kPresets) {
              if (p.type == 1 && p.addr == probe_addrs_[i]) {
                  preview = p.name;
                  found = true;
                  break;
              }
          }
          if (!found) preview = "Addr..."; 
      }

      if (ImGui::BeginCombo("##probe", preview.data())) {
          for (const auto& preset : kPresets) {
              if (preset.name == "-") {
                  ImGui::Separator();
                  continue;
              }
              if (ImGui::Selectable(preset.name.data(), false)) {
                  probe_types_[i] = preset.type;
                  probe_addrs_[i] = preset.addr;
                  UpdateProbe(emu, i, preset.type, preset.addr);
              }
          }
          ImGui::EndCombo();
      }
      
      if (probe_types_[i] == 1) { 
           ImGui::SameLine();
           ImGui::SetNextItemWidth(60);
           char buf[16];
           auto result = std::format_to_n(buf, sizeof(buf)-1, "{:06X}", probe_addrs_[i]);
           *result.out = '\0';
           if (ImGui::InputText("##addr", buf, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
               uint32_t val;
               if (sscanf(buf, "%x", &val) == 1) {
                   probe_addrs_[i] = val;
                   UpdateProbe(emu, i, 1, val);
               }
           }
      }
      ImGui::PopID();
  }

  ImGui::PopStyleVar();
  ImGui::Separator();
}

void LogicAnalyzer::DrawSignal(vamiga::VAmiga& emu, int channel, ImVec2 pos, float width, float height) {
  ImDrawList* dl = ImGui::GetWindowDrawList();
  float dx = width / kSegments;
  float margin = 2.0f;
  float y_mid = pos.y + height * 0.5f;
  float y_top = pos.y + margin;
  float y_bot = pos.y + height - margin;

  dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(50, 50, 50, 255));

  int prev_val = -1;
  int bit_width = (channel == 0) ? 24 : 16; 

  for (int i : std::views::iota(0, kSegments)) {
    int val = data_[channel][i];
    if (val < 0) continue;

    float x1 = pos.x + i * dx;
    float x2 = pos.x + (i + 1) * dx;

    if (bit_width == 1) {
        float y = (val != 0) ? y_top : y_bot;
        if (i > 0 && prev_val != -1) {
            float prev_y = (prev_val != 0) ? y_top : y_bot;
            dl->AddLine(ImVec2(x1, prev_y), ImVec2(x1, y), IM_COL32(200, 200, 200, 255));
        }
        dl->AddLine(ImVec2(x1, y), ImVec2(x2, y), IM_COL32(200, 200, 200, 255));
    } else {
        if (i > 0 && prev_val != val) {
            dl->AddLine(ImVec2(x1, y_bot), ImVec2(x1, y_top), IM_COL32(200, 200, 200, 255));
        }
        dl->AddLine(ImVec2(x1, y_top), ImVec2(x2, y_top), IM_COL32(200, 200, 200, 255));
        dl->AddLine(ImVec2(x1, y_bot), ImVec2(x2, y_bot), IM_COL32(200, 200, 200, 255));
        
        if (dx > 20) {
             std::string label;
             if (hex_mode_) label = std::format("{:X}", val);
             else label = std::format("{}", val);
             
             dl->AddText(ImGui::GetFont(), 10.0f, ImVec2(x1 + 2, y_mid - 5), IM_COL32(200, 200, 200, 255), label.c_str());
        }
    }
    prev_val = val;
  }
}

void LogicAnalyzer::DrawBus(vamiga::VAmiga& emu, ImVec2 pos, float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float dx = width / kSegments;

    for (int i : std::views::iota(0, kSegments)) {
        if (colors_[i] != 0) {
            dl->AddRectFilled(
                ImVec2(pos.x + i * dx, pos.y),
                ImVec2(pos.x + (i + 1) * dx, pos.y + height),
                colors_[i]
            );
        }
    }
    
    float y_text = pos.y + height - 12;
    for (int i : std::views::iota(0, kSegments)) {
        if (dx > 30 && !labels_[i].empty()) {
            dl->AddText(ImGui::GetFont(), 10.0f, ImVec2(pos.x + i * dx + 2, pos.y), IM_COL32(255, 255, 255, 255), labels_[i].c_str());
        }
        
        int val = data_[0][i];
        if (val >= 0 && dx > 40) {
            std::string label = std::format("{:06X}", val);
            dl->AddText(ImGui::GetFont(), 10.0f, ImVec2(pos.x + i * dx + 2, y_text), IM_COL32(200, 200, 200, 255), label.c_str());
        }
    }
}

void LogicAnalyzer::Draw(vamiga::VAmiga& emu) {
  Update(emu);
  DrawControls(emu);

  ImVec2 avail = ImGui::GetContentRegionAvail();
  float content_width = avail.x * zoom_;
  float header_height = 30.0f;
  float row_height = (avail.y - header_height - 20) / kSignals;
  
  ImGui::BeginChild("LogicScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
  
  ImVec2 p = ImGui::GetCursorScreenPos();
  
  ImDrawList* dl = ImGui::GetWindowDrawList();
  float dx = content_width / kSegments;
  
  for (int i : std::views::iota(0, kSegments)) {
     if (i % 4 == 0) {
         std::string s = std::format("{:02X}", i);
         dl->AddText(ImVec2(p.x + i * dx, p.y), IM_COL32(150, 150, 150, 255), s.c_str());
     }
     dl->AddLine(ImVec2(p.x + i*dx, p.y + header_height), ImVec2(p.x + i*dx, p.y + avail.y), IM_COL32(50, 50, 50, 100));
  }
  
  DrawBus(emu, ImVec2(p.x, p.y + header_height), content_width, row_height);
  
  DrawSignal(emu, 1, ImVec2(p.x, p.y + header_height + row_height), content_width, row_height);
  
  for (int c : std::views::iota(2, kSignals)) {
      DrawSignal(emu, c, ImVec2(p.x, p.y + header_height + c * row_height), content_width, row_height);
  }
  
  long hpos = emu.agnus.getInfo().hpos;
  float x_beam = p.x + hpos * dx;
  dl->AddLine(ImVec2(x_beam, p.y), ImVec2(x_beam, p.y + avail.y), IM_COL32(255, 255, 0, 200), 2.0f);

  ImGui::Dummy(ImVec2(content_width, avail.y));
  ImGui::EndChild();
}

}