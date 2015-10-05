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
  };

  static std::vector<Information> availableDevices();

  explicit EvdevDevice(std::vector<std::string> const& paths);
  explicit EvdevDevice(std::initializer_list<std::string> const& paths);
  EvdevDevice(EvdevDevice const&) = delete;
  ~EvdevDevice();
  bool addDevice(std::string const& path);
  PollResult poll();
  bool ready() const;
  bool grab(bool value);

private:
  std::vector<int> _fds;
  std::array<input_event, 64> _events;
  int _numEvents;
  int _currentEvent;
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

EvdevDevice::EvdevDevice(std::initializer_list<std::string> const& paths) :
  _fds(), _events(), _numEvents(0), _currentEvent(0), _previousDevice(0)
{
  for(std::string const& path : paths)
  {
    addDevice(path);
  }
}
EvdevDevice::EvdevDevice(std::vector<std::string> const& paths) :
  _fds(), _events(), _numEvents(0), _currentEvent(0), _previousDevice(0)
{
  for(std::string const& path : paths)
  {
    addDevice(path);
  }
}
EvdevDevice::~EvdevDevice()
{ 
  for(int fd : _fds)
  {
    close(fd);
  }
}
bool EvdevDevice::addDevice(std::string const& path)
{
  if(access(path.data(), F_OK ) != -1)
  {
    int fd = open(path.data(), O_RDONLY | O_NDELAY);
    if(!fd)
    {
      std::cerr << "ERROR: Cannot open '" << path << "'." << std::endl;
      return false;
    }

    _fds.push_back(fd);
    std::cout << "Successfully added device '" << path << "'." << std::endl;
  }
  else
  {
    std::cerr << "ERROR: Cannot access '" << path << "'. Does it exist?" << std::endl;
    return false;
  }

  return true;
}
EvdevDevice::PollResult EvdevDevice::poll()
{
  if(_fds.empty())
    return {POLL_ERROR, {0}};

  if(_currentEvent >= _numEvents)
  {
    // Poll for a ready device
    fd_set fds;
    timeval timeout = {1, 0};
    FD_ZERO(&fds);
    for(int fd : _fds)
    {
      FD_SET(fd, &fds);
    }

    int readyFds = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

    if(readyFds == 0)
    {
      return {POLL_TIMEOUT, {0}};
    }
    else if(readyFds < 0)
    {
      return {POLL_ERROR, {0}};
    }

    // Round-robin next device to read from
    int readyFd = _fds.front();
    if(_fds.size() > 1)
    {
      for(int i = 0; i < _fds.size(); ++i)
      {
        int fd = _fds.at((i + _previousDevice + 1) % _fds.size());
        if(FD_ISSET(fd, &fds))
        {
          readyFd = fd;
          break;
        }
      }
    }

    // Read a set of events from device
    int numBytes = read(readyFd, _events.data(), sizeof(input_event) * _events.size());

    if(numBytes <= 0)
    {
      return {POLL_ERROR, {0}};
    }

    _numEvents = numBytes / sizeof(input_event);
    _currentEvent = 0;
    _previousDevice = readyFd;
  }
  return {POLL_OK, _events[_currentEvent++]};
}

bool EvdevDevice::ready() const
{
  return !_fds.empty();
}
bool EvdevDevice::grab(bool value)
{
  bool success = true;
  for(int fd : _fds)
  {
    success &= ioctl(fd, EVIOCGRAB, value ? 1 : 0) >= 0;
  }

  // If unsuccessful, attempt to revert
  if(!success)
  {
    for(int fd : _fds)
    {
      ioctl(fd, EVIOCGRAB, value ? 0 : 1);
    }
  }

  return success && !_fds.empty();
}

#endif
