#ifndef LINUXGUI_COMPONENTS_INPUT_MANAGER_H_
#define LINUXGUI_COMPONENTS_INPUT_MANAGER_H_
#include <SDL.h>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <utility>
#include "VAmiga.h"
#undef unreachable
#define unreachable std::unreachable()
class InputManager {
 public:
  explicit InputManager(vamiga::VAmiga& emulator);
  ~InputManager();
  void Update();
  void HandleEvent(const SDL_Event& event);
  void HandleWindowFocus(bool focused);
  void SetPortDevices(int port1_device, int port2_device);
  void SetViewportHovered(bool hovered);
  std::string GetDeviceName(int device_id) const;
  
  struct DeviceInfo {
    std::string name;
    std::string manufacturer;
    std::string guid;
    int vendor_id = 0;
    int product_id = 0;
    int version = 0;
    bool is_connected = false;
  };
  DeviceInfo GetDeviceInfo(int device_id) const;
  std::vector<std::string> GetActiveActions(int device_id) const;

  static constexpr int kMaxDevices = 8;
  bool pause_in_background_ = true;
  bool retain_mouse_by_click_ = true;
  bool retain_mouse_by_entering_ = false;
  bool release_mouse_by_shaking_ = true;
 private:
  void SetCaptured(bool captured);
  vamiga::MouseAPI* GetActiveMouse();
  bool HandleKeyset(int device_id, const SDL_KeyboardEvent& event, bool is_down);
  void HandleKeyDown(const SDL_KeyboardEvent& event);
  void HandleKeyUp(const SDL_KeyboardEvent& event);
  void HandleMouseButtonDown(const SDL_MouseButtonEvent& event);
  void HandleMouseButtonUp(const SDL_MouseButtonEvent& event);
  void HandleMouseMotion(const SDL_MouseMotionEvent& event);
  void HandleControllerDeviceAdded(const SDL_ControllerDeviceEvent& event);
  void HandleControllerDeviceRemoved(const SDL_ControllerDeviceEvent& event);
  void HandleControllerButton(const SDL_ControllerButtonEvent& event);
  void HandleControllerAxis(const SDL_ControllerAxisEvent& event);
  bool IsGrabKeyCombo(const SDL_KeyboardEvent& event);
  bool IsReleaseKeyCombo(const SDL_KeyboardEvent& event);
  vamiga::KeyCode SdlToAmigaKeyCode(SDL_Keycode key);
  void UpdateMouse();
  void UpdateJoystick();
  void UpdateGamepads();
  void HandleKeyboard(const SDL_Event& event);
  void UpdateMouseCapture();
  vamiga::VAmiga& emulator_;
  bool window_focused_ = true;
  bool viewport_hovered_ = false;
  bool mouse_captured_ = false;
  bool mouse_buttons_[3] = {false, false, false};
  int port1_device_ = 1;  
  int port2_device_ = 2;
  bool was_paused_by_focus_loss_ = false;
  bool captured_ = false;
  std::map<SDL_JoystickID, std::unique_ptr<SDL_GameController, void (*)(SDL_GameController*)>> controllers_{};
  std::vector<SDL_JoystickID> gamepad_ids_;
};
#endif
