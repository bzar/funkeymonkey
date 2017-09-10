#ifndef EVDEV_DEVICE_H
#define EVDEV_DEVICE_H

#include <linux/input.h>
#include <string>
#include <array>
#include <vector>
#include <iostream>
#include <fstream>
#include <regex>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

namespace
{
  static const std::string DEVICES_INFORMATION_FILE = "/proc/bus/input/devices";
  static const std::string DEV_INPUT = "/dev/input/";
  static const std::regex REGEX_I("I: Bus=([0-9a-f]+) Vendor=([0-9a-f]+) Product=([0-9a-f]+) Version=([0-9a-f]+)");
  static const std::regex REGEX_N("N: Name=\"([^\"]+)\"");
  static const std::regex REGEX_H("H: Handlers=.*(event\\d+).*");
};

class EvdevDevice
{
public:

  struct Input
  {
    Input(std::string const& path, unsigned int role = 0) : path(path), role(role) {}
    std::string path;
    unsigned int role;
  };

  struct Information
  {
    unsigned int bus;
    unsigned int vendor;
    unsigned int product;
    unsigned int version;
    std::string name;
    std::string path;
  };

  enum PollStatus { POLL_OK, POLL_TIMEOUT, POLL_ERROR };
  struct PollResult
  {
    PollStatus status;
    input_event event;
    unsigned int role;
  };

  static std::vector<Information> availableDevices();

  explicit EvdevDevice(std::vector<Input> const& inputs);
  explicit EvdevDevice(std::initializer_list<Input> const& inputs);
  EvdevDevice(EvdevDevice const&) = delete;
  ~EvdevDevice();
  bool addDevice(Input const& path);
  PollResult poll(bool blocking = true);
  bool ready() const;
  bool grab(bool value);

private:
  struct Device
  {
    int fd;
    unsigned int role;
  };
  std::vector<Device> _devices;
  std::array<input_event, 64> _events;
  int _numEvents;
  int _currentEvent;
  unsigned int _currentRole;
  int _previousDevice;
};

std::vector<EvdevDevice::Information> EvdevDevice::availableDevices()
{
  std::ifstream devicesFile(DEVICES_INFORMATION_FILE);
  std::vector<Information> devices;

  while(devicesFile)
  {
    std::string line;
    Information information;
    while(std::getline(devicesFile, line) && !line.empty())
    {
      switch(line.front())
      {
        case 'I':
          {
            std::smatch match;
            if(std::regex_match(line, match, REGEX_I))
            {
              std::istringstream(match[1]) >> std::hex >> information.bus;
              std::istringstream(match[2]) >> std::hex >> information.vendor;
              std::istringstream(match[3]) >> std::hex >> information.product;
              std::istringstream(match[4]) >> std::hex >> information.version;
            }
            break;
          }
        case 'N':
          {
            std::smatch match;
            if(std::regex_match(line, match, REGEX_N))
            {
              information.name = match[1];
            }
            break;
          }
        case 'H':
          {
            std::smatch match;
            if(std::regex_match(line, match, REGEX_H))
            {
              std::ostringstream path;
              path << DEV_INPUT << match[1];
              information.path = path.str();
            }
            break;
          }
       default: break;
      };
    }

    if(!information.path.empty())
    {
      devices.push_back(information);
    }
  }

  return std::move(devices);
}

EvdevDevice::EvdevDevice(std::initializer_list<Input> const& inputs) :
  _devices(), _events(), _numEvents(0), _currentEvent(0), _currentRole(0), _previousDevice(0)
{
  for(Input const& input : inputs)
  {
    addDevice(input);
  }
}
EvdevDevice::EvdevDevice(std::vector<Input> const& inputs) :
  _devices(), _events(), _numEvents(0), _currentEvent(0), _currentRole(0), _previousDevice(0)
{
  for(Input const& input : inputs)
  {
    addDevice(input);
  }
}
EvdevDevice::~EvdevDevice()
{ 
  for(Device const& device : _devices)
  {
    close(device.fd);
  }
}
bool EvdevDevice::addDevice(Input const& input)
{
  if(access(input.path.data(), F_OK ) != -1)
  {
    int fd = open(input.path.data(), O_RDONLY | O_NDELAY);
    if(!fd)
    {
      std::cerr << "ERROR: Cannot open '" << input.path << "'." << std::endl;
      return false;
    }

    _devices.push_back({fd, input.role});
    std::cout << "Successfully added device '" << input.path << "'." << std::endl;
  }

  else
  {
    std::cerr << "ERROR: Cannot access '" << input.path << "'. Does it exist?" << std::endl;
    return false;
  }

  return true;
}
EvdevDevice::PollResult EvdevDevice::poll(bool blocking)
{
  if(_devices.empty())
    return {POLL_ERROR, {0}, 0};

  if(_currentEvent >= _numEvents)
  {
    // Poll for a ready device
    fd_set fds;
    timeval timeout = {1, 0};
    FD_ZERO(&fds);
    for(auto const& device : _devices)
    {
      FD_SET(device.fd, &fds);
    }

    int readyFds = select(FD_SETSIZE, &fds, NULL, NULL, 
        blocking ? NULL : &timeout);

    if(readyFds == 0)
    {
      return {POLL_TIMEOUT, {0}, 0};
    }
    else if(readyFds < 0)
    {
      return {POLL_ERROR, {0}, 0};
    }

    // Round-robin next device to read from
    unsigned int ready = 0;
    if(_devices.size() > 1)
    {
      for(int i = 0; i < _devices.size(); ++i)
      {
        int fd = _devices.at((i + _previousDevice + 1) % _devices.size()).fd;
        if(FD_ISSET(fd, &fds))
        {
          ready = i;
          break;
        }
      }
    }

    Device const& device = _devices.at(ready);

    // Read a set of events from device
    int numBytes = read(device.fd, _events.data(), sizeof(input_event) * _events.size());

    if(numBytes <= 0)
    {
      return {POLL_ERROR, {0}, device.role};
    }

    _numEvents = numBytes / sizeof(input_event);
    _currentEvent = 0;
    _currentRole = device.role;
    _previousDevice = ready;
  }
  return {POLL_OK, _events[_currentEvent++], _currentRole};
}

bool EvdevDevice::ready() const
{
  return !_devices.empty();
}
bool EvdevDevice::grab(bool value)
{
  bool success = true;
  for(auto const& device : _devices)
  {
    success &= ioctl(device.fd, EVIOCGRAB, value ? 1 : 0) >= 0;
  }

  // If unsuccessful, attempt to revert
  if(!success)
  {
    for(auto const& device : _devices)
    {
      ioctl(device.fd, EVIOCGRAB, value ? 0 : 1);
    }
  }

  return success && !_devices.empty();
}

#endif
