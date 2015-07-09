#ifndef EVDEV_DEVICE_H
#define EVDEV_DEVICE_H

#include <linux/input.h>
#include <string>
#include <array>
#include <iostream>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

class EvdevDevice
{
public:
  explicit EvdevDevice(std::string const& path);
  ~EvdevDevice();
  input_event poll();
  bool ready() const;
  bool grab(bool value);
private:
  int _fd;
  std::array<input_event, 64> _events;
  int _numEvents;
  int _currentEvent;
};

EvdevDevice::EvdevDevice(std::string const& path) :
  _fd(0), _events(), _numEvents(0), _currentEvent(0)
{
  if(access(path.data(), F_OK ) != -1)
  {
    _fd = open(path.data(), O_RDONLY | O_NDELAY);
  }
  else
  {
    std::cerr << "ERROR: Cannot access '" << path << "'. Does it exist?" << std::endl;
  }
}
EvdevDevice::~EvdevDevice()
{
  if(_fd)
    close(_fd);
}
input_event EvdevDevice::poll()
{
  if(!_fd)
    return {0};

  if(_currentEvent >= _numEvents)
  {
    fd_set fds;
    timeval timeout = {1, 0};
    FD_ZERO(&fds);
    FD_SET(_fd, &fds);
    if(select(FD_SETSIZE, &fds, NULL, NULL, &timeout ) <= 0)
      return {0};

    int numBytes = read(_fd, _events.data(), sizeof(input_event) * _events.size());
    _numEvents = numBytes / sizeof(input_event);
    _currentEvent = 0;
  }
  return _events[_currentEvent++];
}

bool EvdevDevice::ready() const
{
  return _fd != 0;
}
bool EvdevDevice::grab(bool value)
{
  return _fd && ioctl(_fd, EVIOCGRAB, value ? 1 : 0) >= 0;
}

#endif
