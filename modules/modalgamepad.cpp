#include "funkeymonkeymodule.h"
#include "uinputdevice.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

struct Mouse
{
  // Any thread using device must hold mutex
  UinputDevice device;

  // May be changed from any thread at any time
  int dx;
  int dy;
  int dwx;
  int dwy;

  // Used to signal changes in above values, uses mutex
  std::condition_variable signal;

  // Mouse device mutex
  std::mutex mutex;
};

struct Settings
{
  enum NubAxisMode {
    LEFT_JOYSTICK_X, LEFT_JOYSTICK_Y,
    RIGHT_JOYSTICK_X, RIGHT_JOYSTICK_Y,
    MOUSE_X, MOUSE_Y,
    SCROLL_X, SCROLL_Y
  };
  enum NubClickMode {
    NUB_CLICK_LEFT, NUB_CLICK_RIGHT,
    MOUSE_LEFT, MOUSE_RIGHT
  };

  NubAxisMode leftNubModeX = MOUSE_X;
  NubAxisMode leftNubModeY = MOUSE_Y;
  NubAxisMode rightNubModeX = SCROLL_X;
  NubAxisMode rightNubModeY = SCROLL_Y;
  NubClickMode leftNubClickMode = MOUSE_RIGHT;
  NubClickMode rightNubClickMode = MOUSE_LEFT;

  // May be changed from any thread at any time
  int mouseDeadzone = 100;
  int mouseSensitivity = 15;
  int mouseWheelDeadzone = 500;
};

void handleNubAxis(Settings::NubAxisMode mode, int value, Mouse* mouse, UinputDevice* gamepad);
void handleNubClick(Settings::NubClickMode mode, int value, Mouse* mouse, UinputDevice* gamepad);

// Mouse movement/scroll thread handler
void handleMouse(Mouse* mouse, Settings* settings);

struct
{
  bool stop = false;
  UinputDevice* gamepad = nullptr;
  Mouse* mouse = nullptr;
  std::thread mouseThread;
  Settings settings;
} global;


void init()
{
  global.gamepad = new UinputDevice("/dev/uinput", BUS_USB,
      "Modal Gamepad", 1, 1, 1, {
    { EV_KEY, {
      BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, 
      BTN_TL, BTN_TR, BTN_TL2, BTN_TR2,
      BTN_SELECT, BTN_START, BTN_MODE,
      BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT,
      BTN_THUMBL, BTN_THUMBR
    } },
    { EV_ABS, { REL_X, REL_Y, REL_RX, REL_RY } }
  });
  global.mouse = new Mouse {
    UinputDevice("/dev/uinput", BUS_USB, "Modal Gamepad Mouse", 1, 1, 1, {
        { EV_KEY, { BTN_LEFT, BTN_RIGHT } },
        { EV_REL, { REL_X, REL_Y, REL_HWHEEL, REL_WHEEL } }
        }), 0, 0, 0, 0, {}, {}
  };
  global.mouseThread = std::move(std::thread(handleMouse,
        global.mouse, &global.settings));
}

void handle(input_event const& e)
{
  switch(e.type)
  {
    case EV_ABS:
      switch(e.code)
      {
        case ABS_X:
          handleNubAxis(global.settings.leftNubModeX, e.value,
              global.mouse, global.gamepad);
          break;
        case ABS_Y:
          handleNubAxis(global.settings.leftNubModeY, e.value,
              global.mouse, global.gamepad);
          break;
        case ABS_RX:
          handleNubAxis(global.settings.rightNubModeX, e.value,
              global.mouse, global.gamepad);
          break;
        case ABS_RY:
          handleNubAxis(global.settings.rightNubModeY, e.value,
              global.mouse, global.gamepad);
          break;
        default: break;
      };
      break;
    case EV_KEY:
      switch(e.code)
      {
        case BTN_THUMBL:
          handleNubClick(global.settings.leftNubClickMode, e.value,
              global.mouse, global.gamepad);
          break;
        case BTN_THUMBR:
          handleNubClick(global.settings.rightNubClickMode, e.value,
              global.mouse, global.gamepad);
          break;
        default: break;
      }
  }
}

void destroy()
{
  global.stop = true;
  global.mouse->signal.notify_all();

  if(global.gamepad)
  {
    delete global.gamepad;
  }

  global.mouseThread.join();
}

void handleNubAxis(Settings::NubAxisMode mode, int value, Mouse* mouse, UinputDevice* gamepad)
{
  switch(mode)
  {
    case Settings::MOUSE_X:
      global.mouse->dx = value;
      if(global.mouse->dx > global.settings.mouseDeadzone 
          || global.mouse->dx < -global.settings.mouseDeadzone)
        global.mouse->signal.notify_all();
      break;
    case Settings::MOUSE_Y:
      global.mouse->dy = value;
      if(global.mouse->dy > global.settings.mouseDeadzone 
          || global.mouse->dy < -global.settings.mouseDeadzone)
        global.mouse->signal.notify_all();
      break;
    case Settings::SCROLL_X:
      global.mouse->dwx = value;
      if(global.mouse->dwx > global.settings.mouseDeadzone 
          || global.mouse->dwx < -global.settings.mouseDeadzone)
        global.mouse->signal.notify_all();
      break;
    case Settings::SCROLL_Y:
      global.mouse->dwy = value;
      if(global.mouse->dwy > global.settings.mouseDeadzone 
          || global.mouse->dwy < -global.settings.mouseDeadzone)
        global.mouse->signal.notify_all();
      break;
    case Settings::LEFT_JOYSTICK_X:
      gamepad->send(EV_ABS, ABS_X, value);
      gamepad->send(EV_SYN, 0, 0);
      break;
    case Settings::LEFT_JOYSTICK_Y:
      gamepad->send(EV_ABS, ABS_Y, value);
      gamepad->send(EV_SYN, 0, 0);
      break;
    case Settings::RIGHT_JOYSTICK_X:
      gamepad->send(EV_ABS, ABS_RX, value);
      gamepad->send(EV_SYN, 0, 0);
      break;
    case Settings::RIGHT_JOYSTICK_Y:
      gamepad->send(EV_ABS, ABS_RY, value);
      gamepad->send(EV_SYN, 0, 0);
      break;
  }
}

void handleNubClick(Settings::NubClickMode mode, int value, Mouse* mouse, UinputDevice* gamepad)
{
  switch(mode)
  {
    case Settings::MOUSE_LEFT:
      {
        std::lock_guard<std::mutex> lk(global.mouse->mutex);
        global.mouse->device.send(EV_KEY, BTN_LEFT, value);
        global.mouse->device.send(EV_SYN, 0, 0);
        break;
      }
    case Settings::MOUSE_RIGHT:
      {
        std::lock_guard<std::mutex> lk(global.mouse->mutex);
        global.mouse->device.send(EV_KEY, BTN_RIGHT, value);
        global.mouse->device.send(EV_SYN, 0, 0);
        break;
      }
    case Settings::NUB_CLICK_LEFT:
      global.gamepad->send(EV_KEY, BTN_THUMBL, value);
      global.gamepad->send(EV_SYN, 0, 0);
      break;
    case Settings::NUB_CLICK_RIGHT:
      global.gamepad->send(EV_KEY, BTN_THUMBR, value);
      global.gamepad->send(EV_SYN, 0, 0);
      break;
  }
}

void handleMouse(Mouse* mouse, Settings* settings)
{
  while(!global.stop)
  {
    if(mouse->dx > settings->mouseDeadzone
        || mouse->dx < -settings->mouseDeadzone
        || mouse->dy > settings->mouseDeadzone
        || mouse->dy < -settings->mouseDeadzone
        || mouse->dwx > settings->mouseWheelDeadzone
        || mouse->dwx < -settings->mouseWheelDeadzone
        || mouse->dwy > settings->mouseWheelDeadzone
        || mouse->dwy < -settings->mouseWheelDeadzone)
    {
      std::lock_guard<std::mutex> lk(mouse->mutex);
      if(mouse->dx > settings->mouseDeadzone
          || mouse->dx < -settings->mouseDeadzone)
        mouse->device.send(EV_REL, REL_X, 
            mouse->dx * settings->mouseSensitivity / 1000);
      if(mouse->dy > settings->mouseDeadzone
          || mouse->dy < -settings->mouseDeadzone)
        mouse->device.send(EV_REL, REL_Y,
            mouse->dy * settings->mouseSensitivity / 1000);
      if(mouse->dwx > settings->mouseWheelDeadzone
          || mouse->dwx < -settings->mouseWheelDeadzone)
        mouse->device.send(EV_REL, REL_HWHEEL, mouse->dwx > 0 ? 1 : -1);
      if(mouse->dwy > settings->mouseWheelDeadzone
          || mouse->dwy < -settings->mouseWheelDeadzone)
        mouse->device.send(EV_REL, REL_WHEEL, -mouse->dwy > 0 ? 1 : -1);
      mouse->device.send(EV_SYN, 0, 0);
    }
    else
    {
      std::unique_lock<std::mutex> lk(mouse->mutex);
      mouse->signal.wait(lk);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  delete mouse;
}
