#include "inspector.h"
#include "logic_analyzer.h"
#include <charconv>
#include <cstdio>
#include <format>
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
void Inspector::DrawToolbar(vamiga::VAmiga& emu) {
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
  auto agnus = emu.agnus.getInfo();
  ImGui::TextDisabled("V:%03ld H:%03ld", agnus.vpos, agnus.hpos);
  ImGui::PopStyleVar();
  ImGui::Separator();
}
void Inspector::DrawHexDump(vamiga::VAmiga& emu, uint32_t addr, int rows) {
  ImGui::BeginChild("HexDump", ImVec2(0, rows * ImGui::GetTextLineHeightWithSpacing()));
  for (int r : std::views::iota(0, rows)) {
    uint32_t row_addr = addr + (r * 16);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%08X: ", row_addr);
    for (int c : std::views::iota(0, 16)) {
      ImGui::SameLine();
      uint8_t val = emu.mem.debugger.spypeek8(vamiga::Accessor::CPU, row_addr + c);
      if (hex_mode_) ImGui::Text("%02X", val); else ImGui::Text("%3u", val);
    }
    ImGui::SameLine();
    ImGui::Text(" ");
    for (int c : std::views::iota(0, 16)) {
      ImGui::SameLine();
      uint8_t val = emu.mem.debugger.spypeek8(vamiga::Accessor::CPU, row_addr + c);
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
    vamiga::isize len;
    uint32_t addr = dasm_addr_;
    ImGui::BeginChild("Dasm", ImVec2(0, 200), true);
    for ([[maybe_unused]] int i : std::views::iota(0, 12)) {
      bool is_pc = (addr == cpu.pc0);
      if (is_pc) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
      std::string instr = emu.cpu.debugger.disassembleInstr(addr, &len);
      ImGui::Text("%08X: %s", addr, instr.c_str());
      if (is_pc) ImGui::PopStyleColor();
      addr += len;
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
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%08X", info->addr);
      ImGui::TableSetColumnIndex(2);
      if (ImGui::Button(ICON_FA_TRASH_CAN)) {
        emu.cpu.breakpoints.remove(i);
        ImGui::PopID();
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
void Inspector::DrawMemoryMap(vamiga::VAmiga& emu) {
  ImGui::Text("Bank Map");
  if (ImGui::BeginTable("MemMap", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Bank", ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    auto info = emu.mem.getInfo();
    for (int i : std::views::iota(0, 256)) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      std::string label = std::format("{:02X}", i);
      if (ImGui::Selectable(label.c_str(), (mem_addr_ >> 16) == i, ImGuiSelectableFlags_SpanAllColumns)) {
        mem_addr_ = i << 16;
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", vamiga::MemSrcEnum::help(info.cpuMemSrc[i]));
    }
    ImGui::EndTable();
  }
}
void Inspector::DrawMemory(vamiga::VAmiga& emu) {
  if (ImGui::BeginTable("MemLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("HexDump", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    ImGui::TableSetupColumn("BankMap", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    static char buf[16] = "";
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputText("Address", buf, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
      std::from_chars(buf, buf + 16, mem_addr_, 16);
    }
    ImGui::SameLine();
    if (ImGui::Button("Go")) std::from_chars(buf, buf + 16, mem_addr_, 16);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputText("Find 16-bit", mem_search_buf_, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        uint32_t search_val = 0;
        std::from_chars(mem_search_buf_, mem_search_buf_ + 16, search_val, 16);
        for(uint32_t offset = 0; offset < 0x10000; offset+=2) {
             if (emu.mem.debugger.spypeek16(vamiga::Accessor::CPU, mem_addr_ + offset) == search_val) {
                 mem_addr_ += offset;
                 break;
             }
         }
    }
    DrawHexDump(emu, mem_addr_, 25);
    ImGui::TableSetColumnIndex(1);
    DrawMemoryMap(emu);
    ImGui::EndTable();
  }
}
void Inspector::DrawAgnus(vamiga::VAmiga& emu) {
  auto info = emu.agnus.getInfo();
  if (ImGui::BeginTable("AgnusRegs", 2)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("DMACON", info.dmacon, 16);
    DrawRegister("DDFSTRT", info.ddfstrt, 16);
    DrawRegister("DDFSTOP", info.ddfstop, 16);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("DMA Channels"); ImGui::Separator();
    DrawBit("BLIT", info.dmacon & 0x0040);
    DrawBit("COP", info.dmacon & 0x0080);
    DrawBit("BPL", info.dmacon & 0x0100);
    DrawBit("SPR", info.dmacon & 0x0020);
    DrawBit("DSK", info.dmacon & 0x0010);
    DrawBit("AUD", (info.dmacon & 0x000F));
    ImGui::EndTable();
  }
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
  auto info = emu.paula.getInfo();
  if (ImGui::BeginTable("PaulaRegs", 2)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("INTENA", info.intena, 16);
    DrawRegister("INTREQ", info.intreq, 16);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("ADKCON", info.adkcon, 16);
    ImGui::EndTable();
  }
  ImGui::Separator();
  ImGui::Text("Audio Channels");
  if (ImGui::BeginTable("AudioTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      auto draw_ch = [&](std::string_view label, const vamiga::StateMachineInfo& ch) {
          ImGui::TableNextColumn();
          ImGui::Text("%s", label.data());
          
          ImU32 col = IM_COL32(100, 100, 100, 255);  
          if (ch.state != 0) {
              col = ch.dma ? IM_COL32(50, 200, 50, 255) : IM_COL32(200, 200, 50, 255);
          }
          
          ImVec2 p = ImGui::GetCursorScreenPos();
          ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + 10, p.y + 10), 6.0f, col);
          ImGui::Dummy(ImVec2(20, 20));
          ImGui::SameLine();
          ImGui::Text(ch.dma ? "DMA" : (ch.state ? "Active" : "Idle"));
          
          DrawRegister("VOL", ch.audvol, 8);
          DrawRegister("PER", ch.audper, 16);
          DrawRegister("LEN", ch.audlen, 16);
          DrawRegister("DAT", ch.auddat, 16);
      };
      
      draw_ch("CH0 (L)", emu.paula.audioChannel0.getInfo());
      draw_ch("CH1 (R)", emu.paula.audioChannel1.getInfo());
      draw_ch("CH2 (R)", emu.paula.audioChannel2.getInfo());
      draw_ch("CH3 (L)", emu.paula.audioChannel3.getInfo());
      
      ImGui::EndTable();
  }
}
void Inspector::DrawCIA(vamiga::VAmiga& emu) {
  auto a = emu.ciaA.getInfo();
  auto b = emu.ciaB.getInfo();
  if (ImGui::BeginTable("CIA", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("CIA A (Odd)");
    ImGui::TableSetupColumn("CIA B (Even)");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("PRA", a.portA.reg, 8);
    DrawRegister("DDRA", a.portA.dir, 8);
    DrawRegister("PRB", a.portB.reg, 8);
    DrawRegister("DDRB", a.portB.dir, 8);
    DrawRegister("TA", a.timerA.count, 16);
    DrawRegister("TB", a.timerB.count, 16);
    DrawRegister("SDR", a.sdr, 8);
    DrawRegister("ICR", a.icr, 8);
    DrawRegister("IMR", a.imr, 8);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("PRA", b.portA.reg, 8);
    DrawRegister("DDRA", b.portA.dir, 8);
    DrawRegister("PRB", b.portB.reg, 8);
    DrawRegister("DDRB", b.portB.dir, 8);
    DrawRegister("TA", b.timerA.count, 16);
    DrawRegister("TB", b.timerB.count, 16);
    DrawRegister("SDR", b.sdr, 8);
    DrawRegister("ICR", b.icr, 8);
    DrawRegister("IMR", b.imr, 8);
    ImGui::EndTable();
  }
}
void Inspector::DrawCopper(vamiga::VAmiga& emu) {
  auto info = emu.agnus.getInfo();
  DrawRegister("COPPC", info.coppc0);
  ImGui::Separator();
  ImGui::BeginChild("CopperList");
  uint32_t addr = info.coppc0;
  for ([[maybe_unused]] int i : std::views::iota(0, 16)) {
    std::string dis = emu.agnus.copper.disassemble(addr, true);
    ImGui::Text("%08X: %s", addr, dis.c_str());
    addr += 4;
  }
  ImGui::EndChild();
  DrawCopperBreakpoints(emu);
}
void Inspector::DrawBlitter(vamiga::VAmiga& emu) {
  auto info = emu.agnus.getInfo();
  if (ImGui::BeginTable("BlitRegs", 2)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawRegister("BLTCON0", info.bltcon0, 16);
    ImGui::TableSetColumnIndex(1);
    DrawRegister("BLTAPT", info.bltpt[0]);
    DrawRegister("BLTBPT", info.bltpt[1]);
    DrawRegister("BLTCPT", info.bltpt[2]);
    DrawRegister("BLTDPT", info.bltpt[3]);
    ImGui::EndTable();
  }
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
  if (ImGui::BeginTable("WPTable", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("En", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
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
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%08X", info->addr);
      ImGui::TableSetColumnIndex(2);
      if (ImGui::Button(ICON_FA_TRASH_CAN)) {
        emu.cpu.watchpoints.remove(i);
        ImGui::PopID();
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
         emu.cpu.watchpoints.setAt(addr);
         buf[0] = 0;
      }
    }
    ImGui::TableSetColumnIndex(2);
    ImGui::EndTable();
  }
}

void Inspector::DrawCopperBreakpoints(vamiga::VAmiga& emu) {
    ImGui::Separator();
    ImGui::Text("Copper Breakpoints");
    if (ImGui::BeginTable("CopBPTable", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("En", ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
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
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%08X", info->addr);
      ImGui::TableSetColumnIndex(2);
      if (ImGui::Button(ICON_FA_TRASH_CAN)) {
        emu.copperBreakpoints.remove(i);
        ImGui::PopID();
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
void Inspector::Draw(bool* p_open, vamiga::VAmiga& emu) {
  if (!p_open || !*p_open) return;
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(ICON_FA_MAGNIFYING_GLASS " Inspector", p_open)) {
    if (!emu.isTracking()) emu.trackOn();
    DrawToolbar(emu);
    if (ImGui::BeginTabBar("InspectorTabs")) {
      if (ImGui::BeginTabItem("CPU")) { DrawCPU(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Memory")) { DrawMemory(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Agnus")) { DrawAgnus(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Denise")) { DrawDenise(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Paula")) { DrawPaula(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("CIA")) { DrawCIA(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Copper")) { DrawCopper(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Blitter")) { DrawBlitter(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Events")) { DrawEvents(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Ports")) { DrawPorts(emu); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Bus")) { LogicAnalyzer::Instance().Draw(emu); ImGui::EndTabItem(); }
      ImGui::EndTabBar();
    }
  } else {
    if (emu.isTracking()) emu.trackOff();
  }
  ImGui::End();
}
}
