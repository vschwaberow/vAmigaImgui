#include "console.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>

#include "core_actions.h"
#include "imgui.h"

namespace gui {

Console& Console::Instance() {
  static Console instance;
  return instance;
}

Console::Console()
    : input_buf_(256, 0), scroll_to_bottom_(true), history_pos_(-1) {
}

void Console::SetCommandCallback(std::function<void(const std::string&)> cb) {
  command_callback_ = cb;
}

void Console::ExecCommand(std::string_view command_line) {
  AddLog("# {}\n", command_line);

  history_pos_ = -1;
  for (auto it = history_.begin(); it != history_.end(); ++it) {
    if (*it == command_line) {
      history_.erase(it);
      break;
    }
  }
  history_.emplace_back(command_line);

  if (command_callback_) {
    command_callback_(std::string(command_line));
  } else {
    AddLog("Unknown command: '{}'\n", command_line);
  }

  scroll_to_bottom_ = true;
}

static int TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
  Console* console = (Console*)data->UserData;
  return console->TextEditCallback((void*)data);
}

int Console::TextEditCallback(void* data_void) {
  ImGuiInputTextCallbackData* data = (ImGuiInputTextCallbackData*)data_void;

  switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCompletion: {
      std::string_view buf(data->Buf, (size_t)data->BufTextLen);
      size_t cursor_pos = static_cast<size_t>(data->CursorPos);

      size_t word_start_pos =
          buf.find_last_of(" \t,;", cursor_pos > 0 ? cursor_pos - 1 : 0);
      if (word_start_pos == std::string_view::npos) {
        word_start_pos = 0;
      } else if (word_start_pos < cursor_pos) {
        word_start_pos += 1;
      }

      std::string_view word =
          buf.substr(word_start_pos, cursor_pos - word_start_pos);

      std::vector<std::string> candidates;
      for (const auto& command : commands_) {
        if (word.size() <= command.size() &&
            std::equal(word.begin(), word.end(), command.begin(),
                       [](char a, char b) {
                         return std::tolower((unsigned char)a) ==
                                std::tolower((unsigned char)b);
                       })) {
          candidates.push_back(command);
        }
      }

      if (candidates.empty()) {
        AddLog("No match for \"%s\"!\n", word.data());
      } else if (candidates.size() == 1) {
        data->DeleteChars((int)word_start_pos,
                          (int)(cursor_pos - word_start_pos));
        data->InsertChars(data->CursorPos, candidates[0].c_str());
        data->InsertChars(data->CursorPos, " ");
      } else {
        int match_len = (int)word.size();
        for (;;) {
          int c = 0;
          bool all_candidates_matches = true;
          for (size_t i = 0; i < candidates.size() && all_candidates_matches;
               i++)
            if (i == 0)
              c = std::toupper((unsigned char)candidates[i][match_len]);
            else if (c == 0 ||
                     c != std::toupper((unsigned char)candidates[i][match_len]))
              all_candidates_matches = false;
          if (!all_candidates_matches) break;
          match_len++;
        }

        if (match_len > 0) {
          data->DeleteChars((int)word_start_pos,
                            (int)(cursor_pos - word_start_pos));
          data->InsertChars(data->CursorPos, candidates[0].c_str(),
                            candidates[0].c_str() + match_len);
        }

        AddLog("Possible matches:\n");
        for (const auto& candidate : candidates)
          AddLog("- %s\n", candidate.c_str());
      }

      break;
    }
    case ImGuiInputTextFlags_CallbackHistory: {
      const int prev_history_pos = history_pos_;
      if (data->EventKey == ImGuiKey_UpArrow) {
        if (history_pos_ == -1)
          history_pos_ = (int)history_.size() - 1;
        else if (history_pos_ > 0)
          history_pos_--;
      } else if (data->EventKey == ImGuiKey_DownArrow) {
        if (history_pos_ != -1)
          if (++history_pos_ >= (int)history_.size()) history_pos_ = -1;
      }

      if (prev_history_pos != history_pos_) {
        std::string_view history_str =
            (history_pos_ >= 0) ? history_[history_pos_] : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, history_str.data());
      }
    }
  }
  return 0;
}

bool Console::HandleEvent(const SDL_Event& event, vamiga::VAmiga& emu) {
  if (!has_focus_) return false;

  const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

  if (event.type == SDL_TEXTINPUT) {
    const char* t = event.text.text;
    while (t && *t) {
      emu.retroShell.press(*t++);
    }
    return true;
  }

  if (event.type == SDL_KEYDOWN) {
    switch (event.key.keysym.sym) {
      case SDLK_UP:
        emu.retroShell.press(vamiga::RSKey::UP, shift);
        return true;
      case SDLK_DOWN:
        emu.retroShell.press(vamiga::RSKey::DOWN, shift);
        return true;
      case SDLK_LEFT:
        emu.retroShell.press(vamiga::RSKey::LEFT, shift);
        return true;
      case SDLK_RIGHT:
        emu.retroShell.press(vamiga::RSKey::RIGHT, shift);
        return true;
      case SDLK_PAGEUP:
        emu.retroShell.press(vamiga::RSKey::PAGE_UP, shift);
        return true;
      case SDLK_PAGEDOWN:
        emu.retroShell.press(vamiga::RSKey::PAGE_DOWN, shift);
        return true;
      case SDLK_DELETE:
        emu.retroShell.press(vamiga::RSKey::DEL, shift);
        return true;
      case SDLK_BACKSPACE:
        emu.retroShell.press(vamiga::RSKey::BACKSPACE, shift);
        return true;
      case SDLK_HOME:
        emu.retroShell.press(vamiga::RSKey::HOME, shift);
        return true;
      case SDLK_END:
        emu.retroShell.press(vamiga::RSKey::END, shift);
        return true;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
        emu.retroShell.press(vamiga::RSKey::RETURN, shift);
        return true;
      case SDLK_TAB:
        emu.retroShell.press(vamiga::RSKey::TAB, shift);
        return true;
      default:
        break;
    }
  }

  return false;
}

void Console::Draw(bool* p_open, vamiga::VAmiga& emu) {
  if (!p_open || !*p_open) return;

  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Console", p_open)) {
    ImGui::End();
    return;
  }

  auto info = emu.retroShell.getInfo();
  const char* text_ptr = emu.retroShell.text();
  std::string_view console_text = text_ptr ? text_ptr : "";

  static const ImVec4 text_colors[] = {
      ImVec4(0.81f, 0.81f, 1.0f, 1.0f),   // Commander
      ImVec4(1.0f, 0.81f, 0.81f, 1.0f),   // Debugger
      ImVec4(0.87f, 1.0f, 0.87f, 1.0f)};  // Navigator
  static const ImVec4 bg_colors[] = {
      ImVec4(0.38f, 0.38f, 0.38f, 0.82f),
      ImVec4(0.38f, 0.38f, 0.38f, 0.82f),
      ImVec4(0.38f, 0.38f, 0.38f, 0.82f)};
  static const ImVec4 cursor_color = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);

  const int console_idx = std::clamp<int>(info.console, 0, 2);
  ImGui::PushStyleColor(ImGuiCol_Text, text_colors[console_idx]);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, bg_colors[console_idx]);

  ImGui::TextDisabled("Console: %s",
                      console_idx == 0   ? "Commander"
                      : console_idx == 1 ? "Debugger"
                                         : "Navigator");

  const float footer_height_to_reserve =
      ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("ConsoleText", ImVec2(0, -footer_height_to_reserve),
                    false,
                    ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_AlwaysUseWindowPadding);

  // Compute cursor highlight position (relative to end of text).
  const int cursor_idx =
      std::clamp<int>(static_cast<int>(console_text.size() + info.cursorRel),
                      0, static_cast<int>(console_text.size()));

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 start_pos = ImGui::GetCursorScreenPos();
  ImVec2 pos = start_pos;
  const float line_height = ImGui::GetTextLineHeight();
  ImVec2 highlight_min = pos;
  ImVec2 highlight_max = pos;
  bool have_cursor = false;

  for (int i = 0; i <= static_cast<int>(console_text.size()); ++i) {
    if (i == cursor_idx) {
      have_cursor = true;
      highlight_min = pos;
      const ImVec2 char_size =
          ImGui::CalcTextSize(i < static_cast<int>(console_text.size())
                                  ? &console_text[i]
                                  : " ",
                              i < static_cast<int>(console_text.size())
                                  ? &console_text[i] + 1
                                  : nullptr);
      highlight_max = ImVec2(pos.x + char_size.x, pos.y + line_height);
    }

    if (i == static_cast<int>(console_text.size())) break;

    const char c = console_text[i];
    if (c == '\n') {
      pos.x = start_pos.x;
      pos.y += line_height;
    } else {
      const ImVec2 char_size = ImGui::CalcTextSize(&c, &c + 1);
      pos.x += char_size.x;
    }
  }

  if (have_cursor) {
    draw_list->AddRectFilled(
        highlight_min, highlight_max, ImGui::GetColorU32(cursor_color));
  }

  ImGui::TextUnformatted(console_text.data(),
                         console_text.data() + console_text.size());

  if (console_text.size() != last_text_size_) {
    ImGui::SetScrollHereY(1.0f);
    last_text_size_ = console_text.size();
  }

  has_focus_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  if (has_focus_) {
    SDL_StartTextInput();
  }

  ImGui::EndChild();
  ImGui::PopStyleColor(2);

  ImGui::End();
}

}
