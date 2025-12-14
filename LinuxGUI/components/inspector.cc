#include "inspector.h"
#include "logic_analyzer.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <format>
#include <bitset>
#include <map>
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <ranges>
#include "Components/Agnus/AgnusTypes.h"
#include "Components/CIA/CIATypes.h"
#include "Components/CPU/CPUTypes.h"
#include "Components/Denise/DeniseTypes.h"
#include "Components/Memory/MemoryTypes.h"
#include "Components/Paula/PaulaTypes.h"
#include "Ports/ControlPortTypes.h"
#include "Ports/SerialPortTypes.h"
#include "Components/Paula/UART/UARTTypes.h"
#include "Misc/LogicAnalyzer/LogicAnalyzerTypes.h"
#include "resources/IconsFontAwesome6.h"
namespace gui {
Inspector& Inspector::Instance() {
  static Inspector instance;
  return instance;
}
Inspector::Inspector() {}

void Inspector::OpenWindow() {
  windows_.push_back(WindowState{.open = true, .id = next_id_++, .active_tab = Tab::kCPU});
}

void Inspector::DrawAll(bool* primary_toggle, vamiga::VAmiga& emu) {
  if (primary_toggle) {
    if (*primary_toggle && !windows_.empty()) windows_[0].open = true;
    if (!*primary_toggle && !windows_.empty()) windows_[0].open = false;
  }

  bool any_open = false;
  for (auto& w : windows_) {
    if (w.open) {
      any_open = true;
      DrawWindow(w, emu);
    }
  }

  if (!any_open && emu.isTracking()) {
    emu.trackOff();
  }

  if (windows_.size() > 1) {
    windows_.erase(std::remove_if(windows_.begin() + 1, windows_.end(),
                                  [](const WindowState& w) { return !w.open; }),
                   windows_.end());
  }
  if (windows_.empty()) {
    windows_.push_back(WindowState{.open = primary_toggle ? *primary_toggle : true,
                                   .id = next_id_++, .active_tab = Tab::kCPU});
  }
  if (primary_toggle && !windows_.empty()) {
    *primary_toggle = windows_[0].open;
  }
}

void Inspector::DrawWindow(WindowState& state, vamiga::VAmiga& emu) {
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  std::string title = std::format(ICON_FA_MAGNIFYING_GLASS " Inspector {}", state.id);
  if (ImGui::Begin(title.c_str(), &state.open)) {
    if (!emu.isTracking()) emu.trackOn();
    DrawToolbar(emu, state);
    std::string tab_id = std::format("InspectorTabs{}", state.id);
    if (ImGui::BeginTabBar(tab_id.c_str())) {
      auto tab_item = [&](Tab tab, const char* label) {
        if (ImGui::BeginTabItem(label)) {
          state.active_tab = tab;
          switch (tab) {
            case Tab::kCPU: DrawCPU(emu); break;
            case Tab::kMemory: DrawMemory(emu); break;
            case Tab::kAgnus: DrawAgnus(emu); break;
            case Tab::kDenise: DrawDenise(emu); break;
            case Tab::kPaula: DrawPaula(emu); break;
            case Tab::kCIA: DrawCIA(emu); break;
            case Tab::kCopper: DrawCopper(emu); break;
            case Tab::kBlitter: DrawBlitter(emu); break;
            case Tab::kEvents: DrawEvents(emu); break;
            case Tab::kPorts: DrawPorts(emu); break;
            case Tab::kBus: LogicAnalyzer::Instance().Draw(emu); break;
            case Tab::kNone: break;
          }
          ImGui::EndTabItem();
        }
      };
      tab_item(Tab::kCPU, "CPU");
      tab_item(Tab::kMemory, "Memory");
      tab_item(Tab::kAgnus, "Agnus");
      tab_item(Tab::kDenise, "Denise");
      tab_item(Tab::kPaula, "Paula");
      tab_item(Tab::kCIA, "CIA");
      tab_item(Tab::kCopper, "Copper");
      tab_item(Tab::kBlitter, "Blitter");
      tab_item(Tab::kEvents, "Events");
      tab_item(Tab::kPorts, "Ports");
      tab_item(Tab::kBus, "Bus");
      ImGui::EndTabBar();
    }
  } else {
    if (emu.isTracking()) emu.trackOff();
  }
  ImGui::End();
}
void Inspector::DrawRegister(std::string_view label, uint32_t val, int width) {
  ImGui::Text("%s:", label.data());
  ImGui::SameLine();
  if (hex_mode_) {
    if (width == 8) ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%02X", val);
    else if (width == 16) ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%04X", val);
    else ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%08X", val);
  } else {
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%u", val);
  }
}
void Inspector::DrawBit(std::string_view label, bool set) {
  if (set) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
    ImGui::Text(ICON_FA_SQUARE_CHECK " %s", label.data());
    ImGui::PopStyleColor();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Text(ICON_FA_SQUARE " %s", label.data());
    ImGui::PopStyleColor();
  }
}
void Inspector::DrawToolbar(vamiga::VAmiga& emu, WindowState& state) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 0));
  if (ImGui::Button(emu.isRunning() ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
    if (emu.isRunning()) emu.pause(); else emu.run();
  }
  ImGui::SetItemTooltip(emu.isRunning() ? "Pause" : "Run");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_ARROW_RIGHT)) emu.stepOver();
  ImGui::SetItemTooltip("Step Over");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_ARROW_RIGHT_TO_BRACKET)) emu.stepInto();
  ImGui::SetItemTooltip("Step Into");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FORWARD_STEP)) emu.finishLine();
  ImGui::SetItemTooltip("Finish Line");
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FORWARD_FAST)) emu.finishFrame();
  ImGui::SetItemTooltip("Finish Frame");
  ImGui::SameLine(0, 20);
  if (ImGui::Button(hex_mode_ ? "HEX" : "DEC")) hex_mode_ = !hex_mode_;
  ImGui::SetItemTooltip("Toggle Hex/Dec");
  ImGui::SameLine(0, 20);
  if (ImGui::Button(ICON_FA_PLUS " New")) {
    OpenWindow();
  }
  ImGui::SameLine();
  constexpr std::array presets = {
      "CPU","Memory","Agnus","Denise","Paula","CIA","Copper","Blitter","Events","Ports","Bus"};
  int preset_idx = -1;
  if (ImGui::Combo(
          "Preset", &preset_idx,
          [](void* data, int idx, const char** out_text) {
            auto* arr = static_cast<const std::array<const char*,11>*>(data);
            if (idx < 0 || idx >= static_cast<int>(arr->size())) return false;
            *out_text = (*arr)[static_cast<size_t>(idx)];
            return true;
          },
          (void*)&presets, static_cast<int>(presets.size()))) {
    switch (preset_idx) {
      case 0: state.active_tab = Tab::kCPU; break;
      case 1: state.active_tab = Tab::kMemory; break;
      case 2: state.active_tab = Tab::kAgnus; break;
      case 3: state.active_tab = Tab::kDenise; break;
      case 4: state.active_tab = Tab::kPaula; break;
      case 5: state.active_tab = Tab::kCIA; break;
      case 6: state.active_tab = Tab::kCopper; break;
      case 7: state.active_tab = Tab::kBlitter; break;
      case 8: state.active_tab = Tab::kEvents; break;
      case 9: state.active_tab = Tab::kPorts; break;
      case 10: state.active_tab = Tab::kBus; break;
      default: break;
    }
  }
  ImGui::SameLine(0, 20);
  auto agnus = emu.agnus.getInfo();
  ImGui::TextDisabled("V:%03ld H:%03ld", agnus.vpos, agnus.hpos);
  ImGui::PopStyleVar();
  ImGui::Separator();
}
void Inspector::DrawHexDump(vamiga::VAmiga& emu, uint32_t addr, int rows,
                            vamiga::Accessor accessor) {
  ImGui::BeginChild("HexDump", ImVec2(0, rows * ImGui::GetTextLineHeightWithSpacing()));
  for (int r : std::views::iota(0, rows)) {
    uint32_t row_addr = addr + (r * 16);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%08X: ", row_addr);
    for (int c : std::views::iota(0, 16)) {
      ImGui::SameLine();
      uint8_t val = emu.mem.debugger.spypeek8(accessor, row_addr + c);
      if (hex_mode_) ImGui::Text("%02X", val); else ImGui::Text("%3u", val);
    }
    ImGui::SameLine();
    ImGui::Text(" ");
    for (int c : std::views::iota(0, 16)) {
      ImGui::SameLine();
      uint8_t val = emu.mem.debugger.spypeek8(accessor, row_addr + c);
      if (val >= 32 && val < 127) ImGui::Text("%c", static_cast<char>(val)); else ImGui::TextDisabled(".");
    }
  }
  ImGui::EndChild();
}
void Inspector::DrawCPU(vamiga::VAmiga& emu) {
  auto cpu = emu.cpu.getInfo();
  if (ImGui::BeginTable("CPURegs", 4, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("D0", cpu.d[0]); DrawRegister("D1", cpu.d[1]);
    DrawRegister("D2", cpu.d[2]); DrawRegister("D3", cpu.d[3]);
    DrawRegister("D4", cpu.d[4]); DrawRegister("D5", cpu.d[5]);
    DrawRegister("D6", cpu.d[6]); DrawRegister("D7", cpu.d[7]);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("A0", cpu.a[0]); DrawRegister("A1", cpu.a[1]);
    DrawRegister("A2", cpu.a[2]); DrawRegister("A3", cpu.a[3]);
    DrawRegister("A4", cpu.a[4]); DrawRegister("A5", cpu.a[5]);
    DrawRegister("A6", cpu.a[6]); DrawRegister("A7", cpu.a[7]);
    ImGui::TableSetColumnIndex(2);
    DrawRegister("PC", cpu.pc0); DrawRegister("SR", cpu.sr, 16);
    DrawRegister("USP", cpu.usp); DrawRegister("ISP", cpu.isp);
    DrawRegister("MSP", cpu.msp); DrawRegister("VBR", cpu.vbr);
    DrawRegister("CACR", cpu.cacr); DrawRegister("CAAR", cpu.caar);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("Flags"); ImGui::Separator();
    DrawBit("X - Extend", cpu.sr & 0x10);
    DrawBit("N - Negative", cpu.sr & 0x08);
    DrawBit("Z - Zero", cpu.sr & 0x04);
    DrawBit("V - Overflow", cpu.sr & 0x02);
    DrawBit("C - Carry", cpu.sr & 0x01);
    ImGui::Separator();
    DrawBit("Halted", cpu.halt);
    ImGui::EndTable();
  }
  ImGui::Separator();
  if (ImGui::CollapsingHeader("Trace")) {
    if (ImGui::BeginTable("TraceTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
      ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40);
      ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();
      vamiga::isize count = emu.cpu.debugger.loggedInstructions();
      for (vamiga::isize i : std::views::iota((vamiga::isize)0, count)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%lld", static_cast<long long>(i));
              ImGui::TableSetColumnIndex(1);
              vamiga::isize len;
              auto instr_ptr = emu.cpu.debugger.disassembleRecordedInstr(i, &len);
              std::string_view instr = instr_ptr ? instr_ptr : "?";
              ImGui::Text("%s", instr.data());
            }
            ImGui::EndTable();    }
  }
  ImGui::Separator();
  if (ImGui::CollapsingHeader("Disassembler", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Follow PC", &follow_pc_);
    if (follow_pc_) dasm_addr_ = cpu.pc0;

    constexpr int kLines = 256;
    struct DasmLine {
      uint32_t addr;
      uint32_t len;
      std::string instr;
    };
    std::vector<DasmLine> lines;
    lines.reserve(kLines);

    uint32_t base_addr = static_cast<uint32_t>(std::max(0, dasm_addr_));
    uint32_t addr = base_addr;
    for ([[maybe_unused]] int i : std::views::iota(0, kLines)) {
      vamiga::isize len = 0;
      std::string instr = emu.cpu.debugger.disassembleInstr(addr, &len);
      if (len <= 0) break;
      lines.push_back(DasmLine{addr, static_cast<uint32_t>(len), std::move(instr)});
      addr += static_cast<uint32_t>(len);
    }
    if (lines.empty()) {
      lines.push_back(DasmLine{base_addr, 2, "???"});
      addr = base_addr + 2;
    }
    uint32_t next_page_addr = addr;
    uint32_t page_span = std::max<uint32_t>(2, next_page_addr - base_addr);

    std::array<char, 17> addr_buf{};
    auto addr_str = std::format("{:08X}", base_addr);
    std::ranges::copy(addr_str, addr_buf.begin());
    if (ImGui::InputText("##dasm_addr", addr_buf.data(), addr_buf.size(),
                         ImGuiInputTextFlags_CharsHexadecimal |
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
      uint32_t parsed = base_addr;
      if (auto [_, ec] =
              std::from_chars(addr_buf.data(),
                              addr_buf.data() + std::strlen(addr_buf.data()), parsed, 16);
          ec == std::errc()) {
        dasm_addr_ = static_cast<int>(parsed);
        follow_pc_ = false;
      }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("PC")) {
      follow_pc_ = false;
      dasm_addr_ = static_cast<int>(cpu.pc0);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_ARROW_LEFT)) {
      follow_pc_ = false;
      uint32_t new_addr = (base_addr > page_span) ? base_addr - page_span : 0;
      dasm_addr_ = static_cast<int>(new_addr);
    }
    ImGui::SetItemTooltip("Show previous block");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_ARROW_RIGHT)) {
      follow_pc_ = false;
      dasm_addr_ = static_cast<int>(next_page_addr);
    }
    ImGui::SetItemTooltip("Show next block");
    ImGui::SameLine();
    ImGui::TextDisabled("PC %08X", cpu.pc0);

    ImGui::BeginChild("Dasm", ImVec2(0, 260), true);
    if (ImGui::BeginTable("DasmTable", 3,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableSetupColumn("BP", ImGuiTableColumnFlags_WidthFixed, 36.0f);
      ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();
      for (const auto& line : lines) {
        std::optional<vamiga::GuardInfo> bp = emu.cpu.breakpoints.guardAt(line.addr);
        std::optional<vamiga::GuardInfo> wp = emu.cpu.watchpoints.guardAt(line.addr);
        bool is_bp_enabled = bp && bp->enabled;
        bool is_bp = bp.has_value();
        bool is_wp = wp.has_value();
        ImGui::PushID(line.addr);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (is_bp) {
          ImGui::PushStyleColor(ImGuiCol_Button,
                                is_bp_enabled ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
                                              : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                is_bp_enabled ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
                                              : ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                is_bp_enabled ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
                                              : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        }
        if (ImGui::SmallButton(is_bp ? ICON_FA_CIRCLE : ICON_FA_CIRCLE_DOT)) {
          if (!is_bp) {
            emu.cpu.breakpoints.setAt(line.addr);
          } else if (is_bp_enabled) {
            emu.cpu.breakpoints.disableAt(line.addr);
          } else {
            emu.cpu.breakpoints.enableAt(line.addr);
          }
        }
        if (is_bp && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
          emu.cpu.breakpoints.removeAt(line.addr);
        }
        ImGui::SetItemTooltip(is_bp
                                  ? (is_bp_enabled ? "Disable (right-click to remove)"
                                                   : "Enable (right-click to remove)")
                                  : "Set breakpoint");
        if (is_bp) {
          ImGui::PopStyleColor(3);
        }
        ImGui::TableSetColumnIndex(1);
        bool is_pc = (line.addr == cpu.pc0);
        if (is_pc) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
        if (is_wp) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
        bool selected =
            ImGui::Selectable(std::format("{:08X}", line.addr).c_str(), false,
                              ImGuiSelectableFlags_SpanAllColumns);
        if (is_wp) ImGui::PopStyleColor();
        if (is_pc) ImGui::PopStyleColor();
        if (selected) {
          follow_pc_ = false;
          dasm_addr_ = static_cast<int>(line.addr);
        }
        ImGui::TableSetColumnIndex(2);
        if (is_pc) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
        if (is_wp) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
        ImGui::TextUnformatted(line.instr.c_str());
        if (is_wp) ImGui::PopStyleColor();
        if (is_pc) ImGui::PopStyleColor();
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::EndChild();
  }
  DrawBreakpoints(emu);
  DrawWatchpoints(emu);
}
void Inspector::DrawBreakpoints(vamiga::VAmiga& emu) {
  if (!ImGui::CollapsingHeader("Breakpoints", ImGuiTreeNodeFlags_DefaultOpen)) return;
  if (ImGui::BeginTable("BPTable", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("En", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableHeadersRow();
    int count = emu.cpu.breakpoints.elements();
    for (int i : std::views::iota(0, count)) {
      auto info = emu.cpu.breakpoints.guardNr(i);
      if (!info) continue;
      ImGui::PushID(i);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      bool enabled = info->enabled;
      if (ImGui::Checkbox("##en", &enabled)) {
        emu.cpu.breakpoints.toggle(i);
        SetDasmAddress(static_cast<int>(info->addr));
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          info->enabled ? ImVec4(0.9f, 0.4f, 0.4f, 1.0f)
                        : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
      if (ImGui::Selectable(
              std::format("{:08X}", info->addr).c_str(), false,
              ImGuiSelectableFlags_SpanAllColumns |
                  ImGuiSelectableFlags_AllowDoubleClick)) {
        SetDasmAddress(static_cast<int>(info->addr));
      }
      ImGui::PopStyleColor();
      ImGui::TableSetColumnIndex(2);
      if (ImGui::Button(ICON_FA_TRASH_CAN)) {
        emu.cpu.breakpoints.remove(i);
        ImGui::PopID();
        SetDasmAddress(static_cast<int>(info->addr));
        break; 
      }
      ImGui::PopID();
    }
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("+");
    ImGui::TableSetColumnIndex(1);
    static char buf[16] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##add", buf, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
      uint32_t addr = 0;
      std::from_chars(buf, buf + 16, addr, 16);
      if (addr > 0 || buf[0] == '0') {
         emu.cpu.breakpoints.setAt(addr);
         buf[0] = 0;
      }
    }
    ImGui::TableSetColumnIndex(2);
    ImGui::EndTable();
  }
}
void Inspector::DrawMemoryMap(const vamiga::MemInfo& info, vamiga::Accessor accessor) {
  const auto* src_table = accessor == vamiga::Accessor::AGNUS ? info.agnusMemSrc : info.cpuMemSrc;

  auto color_for = [](vamiga::MemSrc src) -> ImVec4 {
    switch (src) {
      case vamiga::MemSrc::CHIP: return {0.25f, 0.55f, 1.0f, 0.9f};
      case vamiga::MemSrc::CHIP_MIRROR: return {0.25f, 0.55f, 1.0f, 0.4f};
      case vamiga::MemSrc::SLOW: return {0.32f, 0.78f, 0.46f, 0.9f};
      case vamiga::MemSrc::SLOW_MIRROR: return {0.32f, 0.78f, 0.46f, 0.4f};
      case vamiga::MemSrc::FAST: return {0.75f, 0.46f, 0.96f, 0.9f};
      case vamiga::MemSrc::CIA: return {0.93f, 0.76f, 0.32f, 0.9f};
      case vamiga::MemSrc::CIA_MIRROR: return {0.93f, 0.76f, 0.32f, 0.5f};
      case vamiga::MemSrc::RTC: return {0.96f, 0.58f, 0.58f, 0.9f};
      case vamiga::MemSrc::CUSTOM: return {0.94f, 0.33f, 0.33f, 0.9f};
      case vamiga::MemSrc::CUSTOM_MIRROR: return {0.94f, 0.33f, 0.33f, 0.5f};
      case vamiga::MemSrc::AUTOCONF: return {0.56f, 0.56f, 0.56f, 0.9f};
      case vamiga::MemSrc::ZOR: return {0.54f, 0.38f, 0.72f, 0.9f};
      case vamiga::MemSrc::ROM: return {0.98f, 0.52f, 0.18f, 0.9f};
      case vamiga::MemSrc::ROM_MIRROR: return {0.98f, 0.52f, 0.18f, 0.5f};
      case vamiga::MemSrc::WOM: return {0.70f, 0.70f, 0.70f, 0.9f};
      case vamiga::MemSrc::EXT: return {0.29f, 0.77f, 0.77f, 0.9f};
      case vamiga::MemSrc::NONE: return {0.18f, 0.18f, 0.18f, 0.9f};
    }
    return {0.18f, 0.18f, 0.18f, 0.9f};
  };

  auto brighten = [](ImVec4 col, float factor) {
    col.x = std::min(col.x * factor, 1.0f);
    col.y = std::min(col.y * factor, 1.0f);
    col.z = std::min(col.z * factor, 1.0f);
    return col;
  };

  std::array<bool, static_cast<size_t>(vamiga::MemSrc::EXT) + 1> present{};
  for (int i : std::views::iota(0, 256)) {
    auto idx = static_cast<size_t>(src_table[i]);
    if (idx < present.size()) present[idx] = true;
  }

  ImGui::Text("Bank Layout (%s)", accessor == vamiga::Accessor::AGNUS ? "Agnus" : "CPU");
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 2));
  const ImVec2 cell_size(22.0f, 20.0f);
  for (int row : std::views::iota(0, 8)) {
    for (int col : std::views::iota(0, 32)) {
      int bank = row * 32 + col;
      auto src = src_table[bank];
      auto base_col = color_for(src);
      ImGui::PushID(bank);
      ImGui::PushStyleColor(ImGuiCol_Button, base_col);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, brighten(base_col, 1.2f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, brighten(base_col, 1.3f));
      const std::string label = std::format("{:02X}", bank);
      if (ImGui::Button(label.c_str(), cell_size)) {
        mem_selected_bank_ = bank;
        mem_addr_ = static_cast<uint32_t>(bank) << 16;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", vamiga::MemSrcEnum::help(src));
      }
      if (bank == mem_selected_bank_) {
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 2.0f, 0, 2.0f);
      }
      ImGui::PopStyleColor(3);
      ImGui::PopID();
      if (col != 31) ImGui::SameLine(0.0f, 2.0f);
    }
  }
  ImGui::PopStyleVar();

  ImGui::Spacing();
  if (ImGui::BeginTable("MemLegend", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 24.0f);
    ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_WidthStretch);
    const std::array legend_sources = {
      vamiga::MemSrc::CHIP, vamiga::MemSrc::CHIP_MIRROR, vamiga::MemSrc::SLOW, vamiga::MemSrc::SLOW_MIRROR,
      vamiga::MemSrc::FAST, vamiga::MemSrc::CIA, vamiga::MemSrc::CIA_MIRROR, vamiga::MemSrc::RTC,
      vamiga::MemSrc::CUSTOM, vamiga::MemSrc::CUSTOM_MIRROR, vamiga::MemSrc::AUTOCONF, vamiga::MemSrc::ZOR,
      vamiga::MemSrc::ROM, vamiga::MemSrc::ROM_MIRROR, vamiga::MemSrc::WOM, vamiga::MemSrc::EXT
    };
    for (auto src : legend_sources) {
      auto idx = static_cast<size_t>(src);
      if (idx >= present.size() || !present[idx]) continue;
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::ColorButton("##color", color_for(src),
                         ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                         ImVec2(18, 18));
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", vamiga::MemSrcEnum::help(src));
    }
    ImGui::EndTable();
  }
}
void Inspector::DrawMemory(vamiga::VAmiga& emu) {
  const bool running = emu.isRunning();
  const auto& info = running ? emu.mem.getCachedInfo() : emu.mem.getInfo();
  const auto& config = emu.mem.getConfig();

  mem_selected_bank_ = std::clamp(mem_selected_bank_, 0, 255);
  mem_rows_ = std::clamp(mem_rows_, 8, 32);
  mem_addr_ = std::min(mem_addr_, 0xFFFFFFu);

  if (ImGui::RadioButton("CPU", mem_accessor_ == 0)) mem_accessor_ = 0;
  ImGui::SameLine();
  if (ImGui::RadioButton("Agnus", mem_accessor_ == 1)) mem_accessor_ = 1;
  auto accessor = mem_accessor_ == 1 ? vamiga::Accessor::AGNUS : vamiga::Accessor::CPU;
  const auto* src_table = accessor == vamiga::Accessor::AGNUS ? info.agnusMemSrc : info.cpuMemSrc;

  auto sync_address = [&]() {
    mem_selected_bank_ = std::clamp(mem_selected_bank_, 0, 255);
    const uint32_t start = static_cast<uint32_t>(mem_selected_bank_) << 16;
    if (mem_addr_ < start || mem_addr_ >= start + 0x10000) {
      mem_addr_ = start;
    }
    return start;
  };
  sync_address();

  ImGui::SameLine(0.0f, 20.0f);
  ImGui::Text("Chip %d KB  Slow %d KB  Fast %d KB  ROM %d KB  WOM %d KB  EXT %d KB",
              config.chipSize / 1024, config.slowSize / 1024, config.fastSize / 1024,
              config.romSize / 1024, config.womSize / 1024, config.extSize / 1024);

  ImGui::Separator();

  int bank_slider = mem_selected_bank_;
  if (ImGui::SliderInt("Bank", &bank_slider, 0, 255, "%02X")) {
    mem_selected_bank_ = bank_slider;
    mem_addr_ = static_cast<uint32_t>(mem_selected_bank_) << 16;
    sync_address();
  }

  std::map<vamiga::MemSrc, int> first_bank;
  for (int i : std::views::iota(0, 256)) {
    auto src = src_table[i];
    if (src == vamiga::MemSrc::NONE) continue;
    if (!first_bank.contains(src)) first_bank[src] = i;
  }
  const char* src_label = vamiga::MemSrcEnum::help(src_table[mem_selected_bank_]);
  ImGui::SetNextItemWidth(200.0f);
  if (ImGui::BeginCombo("Jump to source", src_label)) {
    for (const auto& [src, bank] : first_bank) {
      const bool selected = bank == mem_selected_bank_;
      if (ImGui::Selectable(std::format("{} {:02X}", vamiga::MemSrcEnum::help(src), bank).c_str(), selected)) {
        mem_selected_bank_ = bank;
        mem_addr_ = static_cast<uint32_t>(bank) << 16;
        sync_address();
      }
    }
    ImGui::EndCombo();
  }

  uint32_t bank_start = sync_address();
  uint32_t address_value = mem_addr_;
  ImGui::SetNextItemWidth(140.0f);
  if (ImGui::InputScalar("Address", ImGuiDataType_U32, &address_value, nullptr, nullptr,
                         "%08X", ImGuiInputTextFlags_CharsHexadecimal)) {
    mem_addr_ = std::min(address_value, 0xFFFFFFu);
    mem_selected_bank_ = static_cast<int>(mem_addr_ >> 16);
    bank_start = sync_address();
  }
  ImGui::SameLine();
  if (ImGui::Button("Bank start")) {
    mem_addr_ = bank_start;
  }
  bank_start = sync_address();
  ImGui::SameLine();
  ImGui::SetNextItemWidth(140.0f);
  if (ImGui::InputText("Find 16-bit", mem_search_buf_, sizeof(mem_search_buf_),
                       ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
    uint32_t search_val = 0;
    auto res = std::from_chars(mem_search_buf_, mem_search_buf_ + sizeof(mem_search_buf_), search_val, 16);
    if (res.ec == std::errc()) {
      for (uint32_t offset = 0; offset < 0x10000; offset += 2) {
        if (emu.mem.debugger.spypeek16(accessor, bank_start + offset) == search_val) {
          mem_addr_ = bank_start + offset;
          break;
        }
      }
    }
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  ImGui::SliderInt("Rows", &mem_rows_, 8, 32);

  if (ImGui::BeginTable("MemLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Layout", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    ImGui::TableSetupColumn("Hex", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawMemoryMap(info, accessor);
    ImGui::TableSetColumnIndex(1);
    bank_start = sync_address();
    ImGui::Text("Bank %02X (%s)  Offset: +0x%04X", mem_selected_bank_,
                vamiga::MemSrcEnum::help(src_table[mem_selected_bank_]),
                static_cast<unsigned>(mem_addr_ - bank_start));
    ImGui::Text("Offset within bank");
    int offset = static_cast<int>(mem_addr_ - bank_start);
    if (ImGui::SliderInt("##offset", &offset, 0, 0xFFFF)) {
      mem_addr_ = bank_start + static_cast<uint32_t>(offset);
    }
    DrawHexDump(emu, mem_addr_, mem_rows_, accessor);
    ImGui::EndTable();
  }
}
void Inspector::DrawAgnus(vamiga::VAmiga& emu) {
  auto info = emu.isRunning() ? emu.agnus.getCachedInfo() : emu.agnus.getInfo();
  ImGui::Text("VPOS: %ld  HPOS: %ld",
              static_cast<long>(info.vpos), static_cast<long>(info.hpos));
  ImGui::Separator();

  const int dmacon = info.dmacon;
  const int bplcon0 = info.bplcon0;

  ImGui::Text("DMA Control");
  DrawRegister("DMACON", dmacon, 16);
  DrawBit("BltPri", dmacon & 0x0400);
  DrawBit("DMAEN", dmacon & 0x0200);
  DrawBit("BPLEN", dmacon & 0x0100);
  DrawBit("COPEN", dmacon & 0x0080);
  DrawBit("BLTEN", dmacon & 0x0040);
  DrawBit("SPREN", dmacon & 0x0020);
  DrawBit("DSKEN", dmacon & 0x0010);
  DrawBit("AUD3EN", dmacon & 0x0008);
  DrawBit("AUD2EN", dmacon & 0x0004);
  DrawBit("AUD1EN", dmacon & 0x0002);
  DrawBit("AUD0EN", dmacon & 0x0001);

  ImGui::Separator();
  ImGui::Text("BPLCON0 / Display Windows");
  DrawRegister("BPLCON0", bplcon0, 16);
  DrawRegister("DIWSTRT", info.diwstrt, 16);
  DrawRegister("DIWSTOP", info.diwstop, 16);
  DrawRegister("DDFSTRT", info.ddfstrt, 16);
  DrawRegister("DDFSTOP", info.ddfstop, 16);

  ImGui::Separator();
  ImGui::Text("Blitter Mods");
  if (ImGui::BeginTable("AgnusBltMods", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("BLTAMOD", info.bltamod, 16);
    DrawRegister("BLTBMOD", info.bltbmod, 16);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("BLTCMOD", info.bltcmod, 16);
    DrawRegister("BLTDMOD", info.bltdmod, 16);
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Bitplane Mods");
  DrawRegister("BPL1MOD", info.bpl1mod, 16);
  DrawRegister("BPL2MOD", info.bpl2mod, 16);

  ImGui::Separator();
  ImGui::Text("Pointers");
  if (ImGui::BeginTable("AgnusPtrs", 3, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("BPL1PT", info.bplpt[0]);
    DrawRegister("BPL2PT", info.bplpt[1]);
    DrawRegister("BPL3PT", info.bplpt[2]);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("BPL4PT", info.bplpt[3]);
    DrawRegister("BPL5PT", info.bplpt[4]);
    DrawRegister("BPL6PT", info.bplpt[5]);
    ImGui::TableSetColumnIndex(2);
    DrawRegister("AUD0PT", info.audpt[0]);
    DrawRegister("AUD1PT", info.audpt[1]);
    DrawRegister("AUD2PT", info.audpt[2]);
    DrawRegister("AUD3PT", info.audpt[3]);
    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::BeginTable("AgnusPtrs2", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("BLTAPT", info.bltpt[0]);
    DrawRegister("BLTBPT", info.bltpt[1]);
    DrawRegister("BLTCPT", info.bltpt[2]);
    DrawRegister("BLTDPT", info.bltpt[3]);
    DrawRegister("COPPC", info.coppc0, 24);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("SPR0PT", info.sprpt[0]);
    DrawRegister("SPR1PT", info.sprpt[1]);
    DrawRegister("SPR2PT", info.sprpt[2]);
    DrawRegister("SPR3PT", info.sprpt[3]);
    DrawRegister("SPR4PT", info.sprpt[4]);
    DrawRegister("SPR5PT", info.sprpt[5]);
    DrawRegister("SPR6PT", info.sprpt[6]);
    DrawRegister("SPR7PT", info.sprpt[7]);
    DrawRegister("DSKPT", info.dskpt);
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Bitplane Enables / BLS");
  const int bpu = (bplcon0 >> 12) & 0x7;
  DrawBit("BPL1", (dmacon & 0x0100) && bpu >= 1);
  DrawBit("BPL2", (dmacon & 0x0100) && bpu >= 2);
  DrawBit("BPL3", (dmacon & 0x0100) && bpu >= 3);
  DrawBit("BPL4", (dmacon & 0x0100) && bpu >= 4);
  DrawBit("BPL5", (dmacon & 0x0100) && bpu >= 5);
  DrawBit("BPL6", (dmacon & 0x0100) && bpu >= 6);
  DrawBit("BLS", info.bls);
}
void Inspector::DrawDenise(vamiga::VAmiga& emu) {
  auto info = emu.denise.getInfo();
  if (ImGui::BeginTable("DeniseRegs", 2)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("BPLCON0", info.bplcon0, 16);
    DrawRegister("BPLCON1", info.bplcon1, 16);
    DrawRegister("BPLCON2", info.bplcon2, 16);
    DrawRegister("CLXDAT", info.clxdat, 16);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("DIWSTRT", info.diwstrt, 16);
    DrawRegister("DIWSTOP", info.diwstop, 16);
    DrawRegister("JOY0DAT", info.joydat[0], 16);
    DrawRegister("JOY1DAT", info.joydat[1], 16);
    ImGui::EndTable();
  }
  ImGui::Separator();
  ImGui::Text("Palette");
  if (ImGui::BeginTable("PaletteTable", 16)) {
      for (int i : std::views::iota(0, 32)) {
          ImGui::TableNextColumn();
          uint32_t c = info.color[i];
          float r = ((c >> 16) & 0xFF) / 255.0f;
          float g = ((c >> 8) & 0xFF) / 255.0f;
          float b = (c & 0xFF) / 255.0f;
          ImGui::ColorButton(std::format("##col{}", i).c_str(), ImVec4(r, g, b, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(20, 20));
          if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Color %d: #%06X", i, c & 0xFFFFFF);
          }
      }
      ImGui::EndTable();
  }
  ImGui::Separator();
  ImGui::Text("Sprites");
  if (ImGui::BeginTable("SpritesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      for (int i : std::views::iota(0, 8)) {
          if (i % 4 == 0) ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(i % 4);
          DrawSprite(info.sprite[i], i);
      }
      ImGui::EndTable();
  }
}
void Inspector::DrawSprite(const vamiga::SpriteInfo& info, int id) {
    ImGui::Text("Sprite %d", id);
    if (info.height == 0) {
        ImGui::TextDisabled("Disabled");
        return;
    }
    ImGui::Text("H: %ld V: %ld-%ld", info.hstrt, info.vstrt, info.vstop);
    ImGui::Text("Height: %ld %s", info.height, info.attach ? "(Attached)" : "");
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    float scale = 2.0f;  
    float w = 16.0f * scale;
    float h = info.height * scale;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(50, 50, 50, 255));
    
    for (int y : std::views::iota(0L, info.height)) {
        uint64_t row_data = info.data[y];
        uint16_t plane0 = row_data & 0xFFFF;
        uint16_t plane1 = (row_data >> 16) & 0xFFFF;
        
        for (int x : std::views::iota(0, 16)) {
            int bit0 = (plane0 >> (15 - x)) & 1;
            int bit1 = (plane1 >> (15 - x)) & 1;
            int color_idx = bit0 + (bit1 * 2);
            
            if (color_idx > 0) {
                 
                int palette_offset = 0;
                switch (id / 2) {
                    case 0: palette_offset = 1; break;  
                    case 1: palette_offset = 5; break;
                    case 2: palette_offset = 9; break;
                    case 3: palette_offset = 13; break;
                }
                 
                int final_idx = palette_offset + (color_idx - 1);  
                if (final_idx >= 0 && final_idx < 16) {
                    uint16_t c = info.colors[final_idx];
                     
                    int r = (c >> 8) & 0xF; r |= (r << 4);
                    int g = (c >> 4) & 0xF; g |= (g << 4);
                    int b = c & 0xF;        b |= (b << 4);
                    
                    draw_list->AddRectFilled(
                        ImVec2(p.x + x * scale, p.y + y * scale),
                        ImVec2(p.x + (x + 1) * scale, p.y + (y + 1) * scale),
                        IM_COL32(r, g, b, 255));
                }
            }
        }
    }
    ImGui::Dummy(ImVec2(w, h));
}

void Inspector::DrawPaula(vamiga::VAmiga& emu) {
  auto paula = emu.paula.getInfo();
  auto dc = emu.paula.diskController.getInfo();
  auto audio0 = emu.paula.audioChannel0.getInfo();
  auto audio1 = emu.paula.audioChannel1.getInfo();
  auto audio2 = emu.paula.audioChannel2.getInfo();
  auto audio3 = emu.paula.audioChannel3.getInfo();

  static constexpr std::array<const char*, 16> irq_names = {
      nullptr,    nullptr,     "TBE",    "DSKBLK", "SOFT", "PORTS",
      "COPER",    "VERTB",     "BLIT",   "AUD0",   "AUD1", "AUD2",
      "AUD3",     "RBF",       "DSKSYN", "EXTER"};

  auto bit_row = [](std::string_view label, uint16_t value,
                    const std::array<const char*, 16>& names) {
    ImGui::TextDisabled("%s", label.data());
    ImGui::SameLine();
    ImGui::BeginGroup();
    for (int bit : std::views::iota(0, 16) | std::views::reverse) {
      const bool set = (value & (1u << bit)) != 0;
      const ImVec4 col =
          set ? ImVec4(0.8f, 0.4f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, col);
      ImGui::Text("%s %s", set ? ICON_FA_SQUARE_CHECK : ICON_FA_SQUARE,
                  names[bit] ? names[bit] : "");
      ImGui::PopStyleColor();
      if (bit % 4 != 0) ImGui::SameLine();
    }
    ImGui::EndGroup();
  };

  bit_row("INTENA", paula.intena, irq_names);
  bit_row("INTREQ", paula.intreq, irq_names);

  ImGui::Separator();
  ImGui::Text("ADKCON");
  ImGui::BeginGroup();
  auto adk_bit = [&](const char* name, uint16_t mask) {
    const bool set = (paula.adkcon & mask) != 0;
    ImGui::Text("%s %s", set ? ICON_FA_SQUARE_CHECK : ICON_FA_SQUARE, name);
  };
  adk_bit("PRECOMP1", 0x4000);
  adk_bit("PRECOMP0", 0x2000);
  adk_bit("MFMPREC", 0x1000);
  adk_bit("UARTBRK", 0x0800);
  adk_bit("WORDSYNC", 0x0400);
  adk_bit("MSBSYNC", 0x0200);
  adk_bit("FAST", 0x0100);
  ImGui::EndGroup();

  ImGui::Separator();
  ImGui::Text("Disk Controller");
  if (ImGui::BeginTable("DiskCtrl", 2, ImGuiTableFlags_RowBg)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("DSKLEN", dc.dsklen, 16);
    DrawBit("DMAEN", dc.dsklen & 0x8000);
    DrawBit("WRITE", dc.dsklen & 0x4000);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("DSKBYTR", dc.dskbytr, 16);
    DrawBit("BYTEREADY", dc.dskbytr & 0x8000);
    DrawBit("DMAON", dc.dskbytr & 0x4000);
    DrawBit("DISKWRITE", dc.dskbytr & 0x2000);
    DrawBit("WORDEQUAL", dc.dskbytr & 0x1000);
    ImGui::EndTable();
  }
  ImGui::Separator();
  ImGui::Text("Drive: DF%d", static_cast<int>(dc.selectedDrive));
  const char* state = "Idle";
  switch (dc.state) {
    case vamiga::DriveDmaState::OFF: state = "Idle"; break;
    case vamiga::DriveDmaState::WAIT: state = "Waiting for sync"; break;
    case vamiga::DriveDmaState::READ: state = "Reading"; break;
    case vamiga::DriveDmaState::WRITE: state = "Writing"; break;
    case vamiga::DriveDmaState::FLUSH: state = "Flushing"; break;
  }
  ImGui::Text("State: %s", state);
  ImGui::Separator();
  ImGui::Text("FIFO (%d)", dc.fifoCount);
  ImGui::BeginGroup();
  for (int idx : std::views::iota(0, static_cast<int>(dc.fifoCount))) {
    if (idx >= 6) break;
    ImGui::Text("%d: %02X", idx, dc.fifo[idx] & 0xFF);
    if (idx < dc.fifoCount - 1) ImGui::SameLine();
  }
  ImGui::EndGroup();
  ImGui::Separator();

  ImGui::Text("Audio Channels");
  if (ImGui::BeginTable("AudioTable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    auto draw_ch = [&](std::string_view label,
                       const vamiga::StateMachineInfo& ch) {
      ImGui::TableNextColumn();
      ImGui::Text("%s", label.data());

      ImU32 col = IM_COL32(100, 100, 100, 255);
      if (ch.state != 0) {
        col = ch.dma ? IM_COL32(50, 200, 50, 255)
                     : IM_COL32(200, 200, 50, 255);
      }
      ImVec2 p = ImGui::GetCursorScreenPos();
      ImGui::GetWindowDrawList()->AddCircleFilled(
          ImVec2(p.x + 10, p.y + 10), 6.0f, col);
      ImGui::Dummy(ImVec2(20, 20));
      ImGui::SameLine();
      ImGui::Text(ch.dma ? "DMA" : (ch.state ? "Active" : "Idle"));

      DrawRegister("VOL", ch.audvolLatch, 8);
      DrawRegister("PER", ch.audperLatch, 16);
      DrawRegister("LEN", ch.audlenLatch, 16);
      DrawRegister("DAT", ch.auddat, 16);
    };

    draw_ch("CH0 (L)", audio0);
    draw_ch("CH1 (R)", audio1);
    draw_ch("CH2 (R)", audio2);
    draw_ch("CH3 (L)", audio3);

    ImGui::EndTable();
  }
}
void Inspector::DrawCIA(vamiga::VAmiga& emu) {
  ImGui::Text("Select CIA");
  ImGui::SameLine();
  ImGui::RadioButton("CIA A", &selected_cia_, 0);
  ImGui::SameLine();
  ImGui::RadioButton("CIA B", &selected_cia_, 1);
  const bool is_cia_a = selected_cia_ == 0;
  const auto cia_info = is_cia_a ? emu.ciaA.getInfo() : emu.ciaB.getInfo();

  static constexpr std::array<const char*, 8> portA_labels_a = {
      "OVL", "LED", "/CHNG", "/WPRO", "/TK0", "/RDY", "GAME0", "GAME1"};
  static constexpr std::array<const char*, 8> portB_labels_a = {
      "DATA0", "DATA1", "DATA2", "DATA3", "DATA4", "DATA5", "DATA6",
      "DATA7"};
  static constexpr std::array<const char*, 8> portA_labels_b = {
      "BUSY", "POUT", "SEL", "/DSR", "/CTS", "/RTS", "/DTR", "/DCD"};
  static constexpr std::array<const char*, 8> portB_labels_b = {
      "/STEP", "DIR", "/SIDE", "/SEL0", "/SEL1", "/SEL2", "/SEL3", "/MTR"};

  const auto& portA_labels =
      is_cia_a ? portA_labels_a : portA_labels_b;
  const auto& portB_labels =
      is_cia_a ? portB_labels_a : portB_labels_b;

  auto show_bits = [](std::string_view title, uint8_t value,
                      const std::array<const char*, 8>& labels) {
    ImGui::TextDisabled("%s", title.data());
    ImGui::SameLine();
    for (int bit : std::views::iota(0, 8) | std::views::reverse) {
      const bool set = (value >> bit) & 1;
      ImGui::PushStyleColor(ImGuiCol_Text,
                            set ? ImVec4(0.2f, 0.9f, 0.2f, 1.0f)
                                : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      ImGui::Text("%s %s", set ? ICON_FA_SQUARE_CHECK : ICON_FA_SQUARE,
                  labels[bit] ? labels[bit] : "");
      ImGui::PopStyleColor();
      if (bit % 2 == 1) ImGui::SameLine();
    }
  };

  ImGui::Separator();
  ImGui::Text("Ports");
  ImGui::BeginGroup();
  DrawRegister("PRA", cia_info.portA.reg, 8);
  ImGui::SameLine();
  ImGui::TextDisabled("[%s]", std::bitset<8>(cia_info.portA.reg).to_string().c_str());
  DrawRegister("DDRA", cia_info.portA.dir, 8);
  ImGui::SameLine();
  ImGui::TextDisabled("[%s]", std::bitset<8>(cia_info.portA.dir).to_string().c_str());
  show_bits("PORTA", cia_info.portA.port, portA_labels);
  DrawRegister("PRB", cia_info.portB.reg, 8);
  ImGui::SameLine();
  ImGui::TextDisabled("[%s]", std::bitset<8>(cia_info.portB.reg).to_string().c_str());
  DrawRegister("DDRB", cia_info.portB.dir, 8);
  ImGui::SameLine();
  ImGui::TextDisabled("[%s]", std::bitset<8>(cia_info.portB.dir).to_string().c_str());
  show_bits("PORTB", cia_info.portB.port, portB_labels);
  ImGui::EndGroup();

  ImGui::Separator();
  ImGui::Text("Timers");
  if (ImGui::BeginTable("CIATimers", 2, ImGuiTableFlags_BordersInnerV)) {
    auto timer_row = [this](const char* name, const vamiga::CIATimerInfo& t) {
      ImGui::Text("%s", name);
      DrawRegister("COUNT", t.count, 16);
      DrawRegister("LATCH", t.latch, 16);
      DrawBit("Running", t.running);
      DrawBit("Toggle", t.toggle);
      DrawBit("PBOUT", t.pbout);
      DrawBit("OneShot", t.oneShot);
    };
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    timer_row("Timer A", cia_info.timerA);
    ImGui::TableSetColumnIndex(1);
    timer_row("Timer B", cia_info.timerB);
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Time of Day");
  uint32_t tod_val = cia_info.tod.value;
  uint32_t alarm_val = cia_info.tod.alarm;
  ImGui::Text("TOD  : %02X:%02X:%02X",
              (tod_val >> 16) & 0xFF, (tod_val >> 8) & 0xFF, tod_val & 0xFF);
  ImGui::Text("Alarm: %02X:%02X:%02X",
              (alarm_val >> 16) & 0xFF, (alarm_val >> 8) & 0xFF,
              alarm_val & 0xFF);
  DrawBit("TOD IRQ Enable", cia_info.todIrqEnable);

  ImGui::Separator();
  ImGui::Text("Interrupts");
  DrawRegister("ICR", cia_info.icr, 8);
  ImGui::SameLine();
  ImGui::TextDisabled("[%s]", std::bitset<8>(cia_info.icr).to_string().c_str());
  DrawRegister("IMR", cia_info.imr, 8);
  ImGui::SameLine();
  ImGui::TextDisabled("[%s]", std::bitset<8>(cia_info.imr).to_string().c_str());
  DrawBit("IRQ line low", !cia_info.irq);

  ImGui::Separator();
  ImGui::Text("Serial");
  DrawRegister("SDR", cia_info.sdr, 8);
  DrawRegister("SSR", cia_info.ssr, 8);
}
void Inspector::DrawCopper(vamiga::VAmiga& emu) {
  auto info = emu.agnus.copper.getInfo();
  if (ImGui::BeginTable("CopperRegs", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("COP1LC", info.cop1lc, 24);
    DrawRegister("COP1INS", info.cop1ins, 16);
    DrawRegister("COPPC", info.coppc0, 24);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("COP2LC", info.cop2lc, 24);
    DrawRegister("COP2INS", info.cop2ins, 16);
    DrawBit("CDANG", info.cdang);
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Lists");
  ImGui::Checkbox("Symbolic L1", &copper_symbolic_[0]);
  ImGui::SameLine();
  ImGui::Checkbox("Symbolic L2", &copper_symbolic_[1]);
  ImGui::SameLine();
  if (ImGui::Button("Find PC")) {
    SetDasmAddress(static_cast<int>(info.coppc0));
  }

  ImGui::SameLine();
  ImGui::TextDisabled("Extra rows: L1 %d  L2 %d", copper_extra_rows_[0],
                      copper_extra_rows_[1]);
  if (ImGui::Button("L1 +")) copper_extra_rows_[0]++;
  ImGui::SameLine();
  if (ImGui::Button("L1 -")) copper_extra_rows_[0] = std::max(0, copper_extra_rows_[0] - 1);
  ImGui::SameLine();
  if (ImGui::Button("L2 +")) copper_extra_rows_[1]++;
  ImGui::SameLine();
  if (ImGui::Button("L2 -")) copper_extra_rows_[1] = std::max(0, copper_extra_rows_[1] - 1);

  ImGui::Separator();
  ImGui::Text("List 1");
  DrawCopperList(1, copper_symbolic_[0], copper_extra_rows_[0], info, emu);
  ImGui::Separator();
  ImGui::Text("List 2");
  DrawCopperList(2, copper_symbolic_[1], copper_extra_rows_[1], info, emu);

  DrawCopperBreakpoints(emu);
}
void Inspector::DrawBlitter(vamiga::VAmiga& emu) {
  auto info = emu.agnus.blitter.getInfo();
  if (ImGui::BeginTable("BlitRegs", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("BLTCON0", info.bltcon0, 16);
    DrawRegister("BLTCON1", info.bltcon1, 16);
    DrawBit("BBUSY", info.bbusy);
    DrawBit("BZERO", info.bzero);
    DrawBit("Use A", info.bltcon0 & 0x0800);
    DrawBit("Use B", info.bltcon0 & 0x0400);
    DrawBit("Use C", info.bltcon0 & 0x0200);
    DrawBit("Use D", info.bltcon0 & 0x0100);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("BLTAPT", info.bltapt);
    DrawRegister("BLTBPT", info.bltbpt);
    DrawRegister("BLTCPT", info.bltcpt);
    DrawRegister("BLTDPT", info.bltdpt);
    DrawRegister("BLTAMOD", info.bltamod, 16);
    DrawRegister("BLTBMOD", info.bltbmod, 16);
    DrawRegister("BLTCMOD", info.bltcmod, 16);
    DrawRegister("BLTDMOD", info.bltdmod, 16);
    ImGui::EndTable();
  }
  ImGui::Separator();

  ImGui::Text("Hold / New / Old");
  if (ImGui::BeginTable("BlitHold", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("A");
    ImGui::TableSetupColumn("B");
    ImGui::TableSetupColumn("C");
    ImGui::TableSetupColumn("D");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); DrawRegister("HOLD", info.ahold, 16);
    ImGui::TableSetColumnIndex(1); DrawRegister("HOLD", info.bhold, 16);
    ImGui::TableSetColumnIndex(2); DrawRegister("HOLD", info.chold, 16);
    ImGui::TableSetColumnIndex(3); DrawRegister("HOLD", info.dhold, 16);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); DrawRegister("OLD", info.aold, 16);
    ImGui::TableSetColumnIndex(1); DrawRegister("OLD", info.bold, 16);
    ImGui::TableSetColumnIndex(0); DrawRegister("NEW", info.anew, 16);
    ImGui::TableSetColumnIndex(1); DrawRegister("NEW", info.bnew, 16);
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Masks");
  if (ImGui::BeginTable("BlitMasks", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("AFWM", info.bltafwm, 16);
    DrawRegister("ALWM", info.bltalwm, 16);
    DrawBit("First Word", info.firstWord);
    DrawBit("Last Word", info.lastWord);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("Mask In", info.anew, 16);
    DrawRegister("Mask Out", info.aold, 16);
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Barrel Shifters");
  if (ImGui::BeginTable("BlitBarrels", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("A In", info.barrelAin, 16);
    DrawRegister("A Shift", info.ash, 4);
    DrawRegister("A Out", info.barrelAout, 16);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("B In", info.barrelBin, 16);
    DrawRegister("B Shift", info.bsh, 4);
    DrawRegister("B Out", info.barrelBout, 16);
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Fill");
  DrawRegister("Fill In", info.fillIn, 16);
  DrawRegister("Fill Out", info.fillOut, 16);
  DrawBit("Fill Enable", info.fillEnable);
  DrawBit("Store to Dest", info.storeToDest);

  ImGui::Separator();
  ImGui::Text("Logic Functions");
  DrawRegister("Minterm", info.minterm, 8);
  DrawRegister("MintermOut", info.mintermOut, 16);
  DrawBit("FCI", info.fci);
  DrawBit("FCO", info.fco);
}
void Inspector::DrawEvents(vamiga::VAmiga& emu) {
  auto info = emu.agnus.getInfo();
  if (ImGui::BeginTable("Events", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Slot");
    ImGui::TableSetupColumn("Event");
    ImGui::TableSetupColumn("Trigger");
    ImGui::TableHeadersRow();
    for (int i : std::views::iota(0, (int)vamiga::SLOT_COUNT)) {
      auto& slot = info.slotInfo[i];
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%s", vamiga::EventSlotEnum::_key(slot.slot));
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", slot.eventName ? slot.eventName : "-");
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%lld", static_cast<long long>(slot.trigger));
    }
    ImGui::EndTable();
  }
}
void Inspector::DrawWatchpoints(vamiga::VAmiga& emu) {
  if (!ImGui::CollapsingHeader("Watchpoints", ImGuiTreeNodeFlags_DefaultOpen)) return;
  if (ImGui::BeginTable("WPTable", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("En", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableHeadersRow();
    int count = emu.cpu.watchpoints.elements();
    for (int i : std::views::iota(0, count)) {
      auto info = emu.cpu.watchpoints.guardNr(i);
      if (!info) continue;
      ImGui::PushID(i + 1000); 
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      bool enabled = info->enabled;
      if (ImGui::Checkbox("##en", &enabled)) {
        emu.cpu.watchpoints.toggle(i);
        SetDasmAddress(static_cast<int>(info->addr));
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          info->enabled ? ImVec4(0.3f, 0.8f, 1.0f, 1.0f)
                        : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
      if (ImGui::Selectable(
              std::format("{:08X}", info->addr).c_str(), false,
              ImGuiSelectableFlags_SpanAllColumns |
                  ImGuiSelectableFlags_AllowDoubleClick)) {
        SetDasmAddress(static_cast<int>(info->addr));
      }
      ImGui::PopStyleColor();
      ImGui::TableSetColumnIndex(2);
      ImGui::PushItemWidth(-1);
      ImGui::SetNextItemWidth(-1);
      static char edit_buf[16] = "";
      {
        std::string formatted = std::format("{:08X}", info->addr);
        const auto len = std::min(formatted.size(), sizeof(edit_buf) - 1);
        std::copy_n(formatted.data(), len, edit_buf);
        edit_buf[len] = '\0';
      }
      if (ImGui::InputText("##edit", edit_buf, sizeof(edit_buf),
                           ImGuiInputTextFlags_CharsHexadecimal |
                               ImGuiInputTextFlags_EnterReturnsTrue)) {
        uint32_t new_addr = 0;
        if (auto [p, ec] =
                std::from_chars(edit_buf, edit_buf + std::strlen(edit_buf),
                                new_addr, 16);
            ec == std::errc()) {
          if (!emu.cpu.watchpoints.guardAt(new_addr)) {
            emu.cpu.watchpoints.moveTo(i, new_addr);
            SetDasmAddress(static_cast<int>(new_addr));
          }
        }
      }
      ImGui::TableSetColumnIndex(3);
      if (ImGui::Button(ICON_FA_TRASH_CAN)) {
        emu.cpu.watchpoints.remove(i);
        ImGui::PopID();
        SetDasmAddress(static_cast<int>(info->addr));
        break; 
      }
      ImGui::PopID();
    }
    
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("+");
    ImGui::TableSetColumnIndex(1);
    static char buf[16] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##add", buf, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
      uint32_t addr = 0;
      std::from_chars(buf, buf + 16, addr, 16);
      if (addr > 0 || buf[0] == '0') {
        auto existing = emu.cpu.watchpoints.guardAt(addr);
        if (!existing) {
          emu.cpu.watchpoints.setAt(addr);
        }
        buf[0] = 0;
      }
    }
    ImGui::TableSetColumnIndex(2);
    ImGui::EndTable();
  }
}

void Inspector::DrawCopperList(int list_idx, bool symbolic, int extra_rows,
                               const vamiga::CopperInfo& info,
                               vamiga::VAmiga& emu) {
  uint32_t start = (list_idx == 1) ? info.copList1Start : info.copList2Start;
  uint32_t end = (list_idx == 1) ? info.copList1End : info.copList2End;
  if (end < start) std::swap(start, end);
  int native_len = static_cast<int>(std::min<uint32_t>((end - start) / 4, 500));
  int total_rows = std::max(0, native_len + extra_rows);
  ImGui::BeginChild(
      std::format("CopperList{}Child", list_idx).c_str(),
      ImVec2(0, 220), true,
      ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 2));
  bool scrolled_to_pc = false;
  for (int i : std::views::iota(0, total_rows)) {
    uint32_t addr = start + static_cast<uint32_t>(i * 4);
    auto bp = emu.copperBreakpoints.guardAt(addr);
    bool is_bp = bp.has_value();
    bool bp_enabled = bp && bp->enabled;
    bool illegal = emu.agnus.copper.isIllegalInstr(addr);
    bool is_pc = (addr == info.coppc0);

    ImGui::PushID(static_cast<int>(addr));
    if (is_bp) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            bp_enabled ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
                                       : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            bp_enabled ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
                                       : ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            bp_enabled ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
                                       : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }
    if (ImGui::SmallButton(is_bp ? ICON_FA_CIRCLE : ICON_FA_CIRCLE_DOT)) {
      if (!is_bp) {
        emu.copperBreakpoints.setAt(addr);
      } else if (bp_enabled) {
        emu.copperBreakpoints.disableAt(addr);
      } else {
        emu.copperBreakpoints.enableAt(addr);
      }
    }
    if (is_bp && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      emu.copperBreakpoints.removeAt(addr);
    }
    if (is_bp) ImGui::PopStyleColor(3);
    ImGui::SameLine();

    if (is_pc) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
    if (illegal) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));

    std::string dis = emu.agnus.copper.disassemble(addr, symbolic);
    ImGui::Text("%08X: %s", addr, dis.c_str());

    if (illegal) ImGui::PopStyleColor();
    if (is_pc) {
      ImGui::PopStyleColor();
      if (!scrolled_to_pc) {
        ImGui::SetScrollHereY();
        scrolled_to_pc = true;
      }
    }
    ImGui::PopID();
  }
  ImGui::PopStyleVar();
  ImGui::EndChild();
}

void Inspector::DrawCopperBreakpoints(vamiga::VAmiga& emu) {
    ImGui::Separator();
    ImGui::Text("Copper Breakpoints");
    if (ImGui::BeginTable("CopBPTable", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("En", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableHeadersRow();
    int count = emu.copperBreakpoints.elements();
    for (int i : std::views::iota(0, count)) {
      auto info = emu.copperBreakpoints.guardNr(i);
      if (!info) continue;
      ImGui::PushID(i + 2000);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      bool enabled = info->enabled;
      if (ImGui::Checkbox("##en", &enabled)) {
        emu.copperBreakpoints.toggle(i);
        Inspector::Instance().SetDasmAddress(info->addr);
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        Inspector::Instance().SetDasmAddress(info->addr);
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          info->enabled ? ImVec4(0.9f, 0.5f, 0.2f, 1.0f)
                        : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
      if (ImGui::Selectable(
              std::format("{:08X}", info->addr).c_str(), false,
              ImGuiSelectableFlags_SpanAllColumns |
                  ImGuiSelectableFlags_AllowDoubleClick)) {
        Inspector::Instance().SetDasmAddress(info->addr);
      }
      ImGui::PopStyleColor();
      ImGui::TableSetColumnIndex(2);
      ImGui::PushItemWidth(-1);
      ImGui::SetNextItemWidth(-1);
      static char cop_edit_buf[16] = "";
      {
        std::string formatted = std::format("{:08X}", info->addr);
        const auto len =
            std::min(formatted.size(), sizeof(cop_edit_buf) - 1);
        std::copy_n(formatted.data(), len, cop_edit_buf);
        cop_edit_buf[len] = '\0';
      }
      if (ImGui::InputText("##cop_edit", cop_edit_buf, sizeof(cop_edit_buf),
                           ImGuiInputTextFlags_CharsHexadecimal |
                               ImGuiInputTextFlags_EnterReturnsTrue)) {
        uint32_t new_addr = 0;
        if (auto [p, ec] =
                std::from_chars(cop_edit_buf,
                                cop_edit_buf + std::strlen(cop_edit_buf),
                                new_addr, 16);
            ec == std::errc()) {
          if (!emu.copperBreakpoints.guardAt(new_addr)) {
            emu.copperBreakpoints.moveTo(i, new_addr);
            Inspector::Instance().SetDasmAddress(new_addr);
          }
        }
      }
      ImGui::TableSetColumnIndex(3);
      if (ImGui::Button(ICON_FA_TRASH_CAN)) {
        emu.copperBreakpoints.remove(i);
        ImGui::PopID();
        Inspector::Instance().SetDasmAddress(info->addr);
        break; 
      }
      ImGui::PopID();
    }
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("+");
    ImGui::TableSetColumnIndex(1);
    static char buf[16] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##add", buf, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
      uint32_t addr = 0;
      std::from_chars(buf, buf + 16, addr, 16);
      if (addr > 0 || buf[0] == '0') {
         emu.copperBreakpoints.setAt(addr);
         buf[0] = 0;
      }
    }
    ImGui::TableSetColumnIndex(2);
    ImGui::EndTable();
  }
}

void Inspector::DrawPorts(vamiga::VAmiga& emu) {
  auto port1 = emu.controlPort1.getInfo();
  auto port2 = emu.controlPort2.getInfo();
  auto serial = emu.serialPort.getInfo();
  auto uart = emu.paula.uart.getInfo();

  ImGui::Text("Control Ports");
  if (ImGui::BeginTable("Ports", 3, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Register");
      ImGui::TableSetupColumn("Port 1 (Mouse/Joy)");
      ImGui::TableSetupColumn("Port 2 (Mouse/Joy)");
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::Text("JOYDAT");
      ImGui::TableSetColumnIndex(1); DrawRegister("##P1JOY", port1.joydat, 16);
      ImGui::TableSetColumnIndex(2); DrawRegister("##P2JOY", port2.joydat, 16);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::Text("POTDAT");
      ImGui::TableSetColumnIndex(1); DrawRegister("##P1POT", port1.potdat, 16);
      ImGui::TableSetColumnIndex(2); DrawRegister("##P2POT", port2.potdat, 16);
      
      ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("POTGO: %04X", port1.potgo);
  ImGui::SameLine();
  DrawBit("OUTRY", port1.potgo & 0x8000); ImGui::SameLine();
  DrawBit("DATRY", port1.potgo & 0x4000); ImGui::SameLine();
  DrawBit("OUTRX", port1.potgo & 0x2000); ImGui::SameLine();
  DrawBit("DATRX", port1.potgo & 0x1000);

  ImGui::Separator();
  ImGui::Text("Serial Port");
  if (ImGui::BeginTable("Serial", 2)) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      DrawRegister("SERPER", uart.serper, 16);
      ImGui::Text("Baud: %ld", uart.baudRate);
      DrawRegister("RecShift", uart.receiveShiftReg, 16);
      DrawRegister("RecBuff", uart.receiveBuffer, 16);
      
      ImGui::TableSetColumnIndex(1);
      DrawBit("TXD", serial.txd);
      DrawBit("RXD", serial.rxd);
      DrawBit("CTS", serial.cts);
      DrawBit("DSR", serial.dsr);
      DrawBit("CD", serial.cd);
      DrawBit("DTR", serial.dtr);
      DrawRegister("TransShift", uart.transmitShiftReg, 16);
      DrawRegister("TransBuff", uart.transmitBuffer, 16);
      
      ImGui::EndTable();
  }
}
}
