#include "funkeymonkeymodule.h"

#include <iostream>
#include <cstdlib>
#include <array>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <linux/input.h>
#include <dlfcn.h>
#include <signal.h>
#include <string.h>
 
volatile sig_atomic_t done = 0;
 
void term(int signum)
{
  done = 1;
}
 
class EvdevDevice
{
public:
  explicit EvdevDevice(std::string const& path);
  ~EvdevDevice();
  input_event poll();
private:
  int _fd;
  std::array<input_event, 64> _events;
  int _numEvents;
  int _currentEvent;
};

class FunKeyMonkeyModule
{
public:
  FunKeyMonkeyModule(std::string const& path);
  ~FunKeyMonkeyModule();
  void init();
  void handle(input_event const& e);
  void destroy();
private:
  void *_lib;
  void (*_init)();
  void (*_handle)(input_event const&);
  void (*_destroy)();
};

int main(int argc, char** argv)
{
  if(argc < 3)
    return EXIT_FAILURE;

  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = term;
  sigaction(SIGINT, &action, NULL);

  EvdevDevice evdev(argv[1]);
  FunKeyMonkeyModule module(argv[2]);

  module.init();

  while(!done)
  {
    input_event e = evdev.poll();
    module.handle(e);
  }

  module.destroy();

  return EXIT_SUCCESS;
}

EvdevDevice::EvdevDevice(std::string const& path) :
  _fd(0), _events(), _numEvents(0), _currentEvent(0)
{
  _fd = open(path.data(), O_RDONLY | O_NDELAY);
}
EvdevDevice::~EvdevDevice()
{
  close(_fd);
}
input_event EvdevDevice::poll()
{
  if(_currentEvent >= _numEvents)
  {
    fd_set fds;
    timeval timeout;
    do
    {
      if(done)
        return {};
      FD_ZERO(&fds);
      FD_SET(_fd, &fds);
      timeout = {0, 5000};
    } while(select(FD_SETSIZE, &fds, NULL, NULL, &timeout ) <= 0);

    int numBytes = read(_fd, _events.data(), sizeof(input_event) * _events.size());
    _numEvents = numBytes / sizeof(input_event);
    _currentEvent = 0;
  }
  return _events[_currentEvent++];
}


FunKeyMonkeyModule::FunKeyMonkeyModule(std::string const& path) :
  _lib(nullptr), _init(nullptr), _handle(nullptr), _destroy(nullptr)
{
  _lib = dlopen(path.data(), RTLD_LAZY);
  if (!_lib) 
  {
    std::cerr << "ERROR: Error opening module " << path << ": " << dlerror() << std::endl;
  }
  else
  {
    auto load = [this](std::string const& name) {
      void* value = dlsym(_lib, name.data());
      auto error = dlerror();
      if(error)
      {
        std::cerr << "ERROR: While loading " << name << ": " << error << std::endl;   
        value = nullptr;
      }
      return value;
    };
    _init = reinterpret_cast<decltype(_init)>(load("init"));
    _handle = reinterpret_cast<decltype(_handle)>(load("handle"));
    _destroy =reinterpret_cast<decltype(_destroy)>(load("destroy"));
  }
}
FunKeyMonkeyModule::~FunKeyMonkeyModule()
{
  dlclose(_lib);
}
void FunKeyMonkeyModule::init()
{
  if(_init)
    (*_init)();
}
void FunKeyMonkeyModule::handle(input_event const& e)
{
  if(_handle)
    (*_handle)(e);

}
void FunKeyMonkeyModule::destroy()
{
  if(_destroy)
    (*_destroy)();
}
