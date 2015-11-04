#ifndef UINPUTDEVICE_H
#define UINPUTDEVICE_H

#include <linux/input.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <linux/uinput.h>

#include <iostream>
#include <string>
#include <vector>

class UinputDevice
{
public:
  struct PossibleEvent
  {
    unsigned int type;
    std::vector<unsigned int> codes;
  };
  explicit UinputDevice(std::string const& path, unsigned int bus, std::string const& name, unsigned int vendor, unsigned int product, unsigned int version, std::vector<PossibleEvent> const& possibleEvents);
  ~UinputDevice();
  bool send(unsigned int type, unsigned int code, int value);
  bool ready() const; 
  operator bool() const; 
  void destroy();
private:
  int _fd;
};

UinputDevice::UinputDevice(std::string const& path, unsigned int bus, std::string const& name, unsigned int vendor, unsigned int product, unsigned int version, std::vector<PossibleEvent> const& possibleEvents)
{
  _fd = ::open(path.data(), O_WRONLY | O_NONBLOCK);
  if(_fd)
  {
    uinput_user_dev device;
    memset(&device,0,sizeof(device));
    strncpy(device.name, name.data(), UINPUT_MAX_NAME_SIZE);
    device.id.bustype = bus;
    device.id.product = product;
    device.id.vendor = vendor;
    device.id.version = version;

    if(write(_fd, &device, sizeof(device)) != sizeof(device))
    {
      close(_fd);
      _fd = 0;
    }
    else
    {
      for(PossibleEvent const& pe : possibleEvents)
      {
        if(ioctl(_fd, UI_SET_EVBIT, pe.type) < 0)
        {
          std::cerr << "ERROR: ioctl error adding event type " << pe.type << std::endl;
        }

        for(int code : pe.codes)
        {
          unsigned int type = 0;
          switch(pe.type)
          {
            case EV_KEY: type = UI_SET_KEYBIT; break;
            case EV_REL: type = UI_SET_RELBIT; break;
            case EV_ABS: type = UI_SET_ABSBIT; break;
            default: std:: cerr << "ERROR: Unsupported event type " << pe.type << std::endl;
          }

          if(type && ioctl(_fd, type, code) < 0)
          {
            std::cerr << "ERROR: ioctl error adding event code " << pe.type << " " << code << std::endl;
          }
        }
      }

      if(ioctl(_fd, UI_DEV_CREATE) < 0)
      {
        std::cerr << "ERROR: ioctl error creating device" << std::endl;
      }
    }
  }
}
UinputDevice::~UinputDevice()
{
  destroy();
}
bool UinputDevice::send(unsigned int type, unsigned int code, int value)
{
  if(!_fd)
    return false;

  input_event event;
  memset(&event,0,sizeof(event));
  event.type = type;
  event.code = code;
  event.value = value;
  return write(_fd, &event, sizeof(event)) == sizeof(event);
}

bool UinputDevice::ready() const
{
  return _fd != 0;
}
UinputDevice::operator bool() const
{
  return ready();
}
void UinputDevice::destroy()
{
  if(_fd)
  {
    ioctl(_fd, UI_DEV_DESTROY);
    close(_fd);
    _fd = 0;
  }
}
#endif
