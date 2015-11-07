#include "funkeymonkeymodule.h"
#include "uinputdevice.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>

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
    UNKNOWN_NUB_AXIS_MODE,
    LEFT_JOYSTICK_X, LEFT_JOYSTICK_Y,
    RIGHT_JOYSTICK_X, RIGHT_JOYSTICK_Y,
    MOUSE_X, MOUSE_Y,
    SCROLL_X, SCROLL_Y
  };
  enum NubClickMode {
    UNKNOWN_NUB_CLICK_MODE,
    NUB_CLICK_LEFT, NUB_CLICK_RIGHT,
    MOUSE_LEFT, MOUSE_RIGHT
  };

  NubAxisMode leftNubModeX = LEFT_JOYSTICK_X;
  NubAxisMode leftNubModeY = LEFT_JOYSTICK_Y;
  NubAxisMode rightNubModeX = RIGHT_JOYSTICK_X;
  NubAxisMode rightNubModeY = RIGHT_JOYSTICK_Y;
  NubClickMode leftNubClickMode = NUB_CLICK_LEFT;
  NubClickMode rightNubClickMode = NUB_CLICK_RIGHT;

  // May be changed from any thread at any time
  int mouseDeadzone = 100;
  int mouseSensitivity = 15;
  int mouseWheelDeadzone = 500;

  std::string configFile;
};

using NubAxisModeMap = std::unordered_map<std::string, Settings::NubAxisMode>;
NubAxisModeMap const NUB_AXIS_MODES = {
{ "left_joystick_x", Settings::LEFT_JOYSTICK_X },
{ "left_joystick_y", Settings::LEFT_JOYSTICK_Y },
{ "right_joystick_x", Settings::RIGHT_JOYSTICK_X },
{ "right_joystick_y", Settings::RIGHT_JOYSTICK_Y },
  { "mouse_x", Settings::MOUSE_X },
  { "mouse_y", Settings::MOUSE_Y },
  { "scroll_x", Settings::SCROLL_X },
  { "scroll_y", Settings::SCROLL_Y }
};

using NubClickModeMap = std::unordered_map<std::string, Settings::NubClickMode>;
NubClickModeMap const NUB_CLICK_MODES = {
  { "nub_click_left", Settings::NUB_CLICK_LEFT },
  { "nub_click_right", Settings::NUB_CLICK_RIGHT },
  { "mouse_left", Settings::MOUSE_LEFT },
  { "mouse_right", Settings::MOUSE_RIGHT }
};

void handleArgs(char const** argv, unsigned int argc, Settings& settings);
void loadConfig(std::string const& filename, Settings& settings);
Settings::NubAxisMode parseNubAxisMode(std::string const& str);
Settings::NubClickMode parseNubClickMode(std::string const& str);
void handleNubAxis(Settings::NubAxisMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings);
void handleNubClick(Settings::NubClickMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings);

// Mouse movement/scroll thread handler
void handleMouse(Mouse* mouse, Settings* settings, bool* stop);

struct
{
  bool stop = false;
  UinputDevice* gamepad = nullptr;
  Mouse* mouse = nullptr;
  std::thread mouseThread;
  Settings settings;
} global;


void init(char const** argv, unsigned int argc)
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
        global.mouse, &global.settings, &global.stop));

  handleArgs(argv, argc, global.settings);

  if(!global.settings.configFile.empty())
  {
    loadConfig(global.settings.configFile, global.settings);
  }
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
              global.mouse, global.gamepad, global.settings);
          break;
        case ABS_Y:
          handleNubAxis(global.settings.leftNubModeY, e.value,
              global.mouse, global.gamepad, global.settings);
          break;
        case ABS_RX:
          handleNubAxis(global.settings.rightNubModeX, e.value,
              global.mouse, global.gamepad, global.settings);
          break;
        case ABS_RY:
          handleNubAxis(global.settings.rightNubModeY, e.value,
              global.mouse, global.gamepad, global.settings);
          break;
        default: break;
      };
      break;
    case EV_KEY:
      switch(e.code)
      {
        case BTN_THUMBL:
          handleNubClick(global.settings.leftNubClickMode, e.value,
              global.mouse, global.gamepad, global.settings);
          break;
        case BTN_THUMBR:
          handleNubClick(global.settings.rightNubClickMode, e.value,
              global.mouse, global.gamepad, global.settings);
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

void handleArgs(char const** argv, unsigned int argc, Settings& settings)
{
  std::string configFile;
  std::regex configRegex("config=(.*)");

  for(unsigned int i = 0; i < argc; ++i)
  {
    std::string arg(argv[i]);
    std::smatch match;
    if(std::regex_match(arg, match, configRegex))
    {
      settings.configFile = match[1];
    }
  }
}

using SettingHandler = std::function<void(std::string const&,Settings&)>;
using SettingHandlerMap = std::unordered_map<std::string, SettingHandler>;
SettingHandlerMap const SETTING_HANDLERS = {
  { "mouse.sensitivity", [](std::string const& value, Settings& settings)
    {
      settings.mouseSensitivity = std::stoi(value);
    }
  },
  { "mouse.deadzone", [](std::string const& value, Settings& settings)
    {
      settings.mouseDeadzone = std::stoi(value);
    }
  },
  { "mouse.wheel.deadzone", [](std::string const& value, Settings& settings)
    {
      settings.mouseWheelDeadzone = std::stoi(value);
    }
  },
  { "nubs.left.x", [](std::string const& value, Settings& settings)
    {
      settings.leftNubModeX = parseNubAxisMode(value);
    }
  },
  { "nubs.left.y", [](std::string const& value, Settings& settings)
    {
      settings.leftNubModeY = parseNubAxisMode(value);
    }
  },
  { "nubs.right.x", [](std::string const& value, Settings& settings)
    {
      settings.rightNubModeX = parseNubAxisMode(value);
    }
  },
  { "nubs.right.y", [](std::string const& value, Settings& settings)
    {
      settings.rightNubModeY = parseNubAxisMode(value);
    }
  },
  { "nubs.left.click", [](std::string const& value, Settings& settings)
    {
      settings.leftNubClickMode = parseNubClickMode(value);
    }
  },
  { "nubs.right.click", [](std::string const& value, Settings& settings)
    {
      settings.rightNubClickMode = parseNubClickMode(value);
    }
  }
};

void loadConfig(std::string const& filename, Settings& settings)
{
  std::regex re("^([\\w.]+)\\s*=\\s*(.*)$");
  std::regex emptyRe("^\\s*$");
  std::ifstream configFile(filename);

  if(!configFile)
  {
    std::cerr << "ERROR: Could not open config file " << filename << std::endl;
    return;
  }
  std::string line;
  while(std::getline(configFile, line))
  {
    if(line.empty() || line.at(0) == '#' || std::regex_match(line, emptyRe))
      continue;

    std::smatch match;
    if(std::regex_match(line, match, re))
    {
      std::string key = match[1];
      std::string value = match[2];
      std::transform(key.begin(), key.end(), key.begin(), tolower);
      auto iter = SETTING_HANDLERS.find(key);
      if(iter == SETTING_HANDLERS.end())
      {
        std::cout << "WARNING: Unknown setting in config file: " 
          << key << std::endl;
      }
      else
      {
        iter->second(value, settings);
      }
    }
    else
    {
      std::cerr << "Invalid line in config file: " << line << std::endl;
    }
  }
}

Settings::NubAxisMode parseNubAxisMode(std::string const& str)
{
  std::string s(str);
  std::transform(s.begin(), s.end(), s.begin(), tolower);
  auto iter = NUB_AXIS_MODES.find(s);
  if(iter == NUB_AXIS_MODES.end())
  {
    return Settings::UNKNOWN_NUB_AXIS_MODE;
  }
  else
  {
    return iter->second;
  }
}

Settings::NubClickMode parseNubClickMode(std::string const& str)
{
  std::string s(str);
  std::transform(s.begin(), s.end(), s.begin(), tolower);
  auto iter = NUB_CLICK_MODES.find(s);
  if(iter == NUB_CLICK_MODES.end())
  {
    return Settings::UNKNOWN_NUB_CLICK_MODE;
  }
  else
  {
    return iter->second;
  }
}

void handleNubAxis(Settings::NubAxisMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings)
{
  switch(mode)
  {
    case Settings::MOUSE_X:
      mouse->dx = value;
      if(mouse->dx > settings.mouseDeadzone 
          || mouse->dx < -settings.mouseDeadzone)
        mouse->signal.notify_all();
      break;
    case Settings::MOUSE_Y:
      mouse->dy = value;
      if(mouse->dy > settings.mouseDeadzone 
          || mouse->dy < -settings.mouseDeadzone)
        mouse->signal.notify_all();
      break;
    case Settings::SCROLL_X:
      mouse->dwx = value;
      if(mouse->dwx > settings.mouseDeadzone 
          || mouse->dwx < -settings.mouseDeadzone)
        mouse->signal.notify_all();
      break;
    case Settings::SCROLL_Y:
      mouse->dwy = value;
      if(mouse->dwy > settings.mouseDeadzone 
          || mouse->dwy < -settings.mouseDeadzone)
        mouse->signal.notify_all();
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
    case Settings::UNKNOWN_NUB_AXIS_MODE:
      break;
  }
}

void handleNubClick(Settings::NubClickMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings)
{
  switch(mode)
  {
    case Settings::MOUSE_LEFT:
      {
        std::lock_guard<std::mutex> lk(mouse->mutex);
        mouse->device.send(EV_KEY, BTN_LEFT, value);
        mouse->device.send(EV_SYN, 0, 0);
        break;
      }
    case Settings::MOUSE_RIGHT:
      {
        std::lock_guard<std::mutex> lk(mouse->mutex);
        mouse->device.send(EV_KEY, BTN_RIGHT, value);
        mouse->device.send(EV_SYN, 0, 0);
        break;
      }
    case Settings::NUB_CLICK_LEFT:
      gamepad->send(EV_KEY, BTN_THUMBL, value);
      gamepad->send(EV_SYN, 0, 0);
      break;
    case Settings::NUB_CLICK_RIGHT:
      gamepad->send(EV_KEY, BTN_THUMBR, value);
      gamepad->send(EV_SYN, 0, 0);
      break;
  case Settings::UNKNOWN_NUB_CLICK_MODE:
      break;
  }
}

void handleMouse(Mouse* mouse, Settings* settings, bool* stop)
{
  while(!*stop)
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

      if(mouse->dx > settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_X, 
            (mouse->dx - settings->mouseDeadzone)
            * settings->mouseSensitivity / 1000);
      }
      else if(mouse->dx < -settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_X, 
            (mouse->dx + settings->mouseDeadzone)
            * settings->mouseSensitivity / 1000);
      }

      if(mouse->dy > settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_Y, 
            (mouse->dy - settings->mouseDeadzone)
            * settings->mouseSensitivity / 1000);
      }
      else if(mouse->dy < -settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_Y, 
            (mouse->dy + settings->mouseDeadzone)
            * settings->mouseSensitivity / 1000);
      }

      if(mouse->dwx > settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_HWHEEL, 1);
      }
      else if(mouse->dwx < -settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_HWHEEL, -1);
      }

      if(mouse->dwy > settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_WHEEL, -1);
      }
      else if(mouse->dwy < -settings->mouseDeadzone)
      {
        mouse->device.send(EV_REL, REL_WHEEL, 1);
      }

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

