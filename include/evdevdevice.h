#ifndef EVDEV_DEVICE_H
#define EVDEV_DEVICE_H

#include <linux/input.h>
#include <string>
#include <array>
#include <vector>
#include <iostream>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

class EvdevDevice
{
public:
  explicit EvdevDevice(std::vector<std::string> const& paths);
  explicit EvdevDevice(std::initializer_list<std::string> const& paths);
  EvdevDevice(EvdevDevice const&) = delete;
  ~EvdevDevice();
  bool addDevice(std::string const& path);
  input_event poll();
  bool ready() const;
  bool grab(bool value);
private:
  std::vector<int> _fds;
  std::array<input_event, 64> _events;
  int _numEvents;
  int _currentEvent;
  int _previousDevice;
};

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
input_event EvdevDevice::poll()
{
  if(_fds.empty())
    return {0};

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
    if(select(FD_SETSIZE, &fds, NULL, NULL, &timeout ) <= 0)
      return {0};

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
    _numEvents = numBytes / sizeof(input_event);
    _currentEvent = 0;
    _previousDevice = readyFd;
  }
  return _events[_currentEvent++];
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
