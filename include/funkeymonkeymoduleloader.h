#ifndef FUNKEYMONKEY_MODULE_LOADER_H
#define FUNKEYMONKEY_MODULE_LOADER_H

#include <linux/input.h>
#include <string>
#include <iostream>
#include <dlfcn.h>

class FunKeyMonkeyModule
{
public:
  FunKeyMonkeyModule(std::string const& path);
  ~FunKeyMonkeyModule();
  bool ready() const;
  void init(char const** argv, unsigned int argc);
  void handle(input_event const& e, int src);
  void destroy();
  void user1();
  void user2();
private:
  void *_lib;
  void (*_init)(char const**, unsigned int);
  void (*_handle)(input_event const&, int src);
  void (*_destroy)();
  void (*_user1)();
  void (*_user2)();
};

FunKeyMonkeyModule::FunKeyMonkeyModule(std::string const& path) :
  _lib(nullptr), _init(nullptr), _handle(nullptr), _destroy(nullptr)
{
  char* absPath = realpath(path.data(), nullptr);
  if(absPath)
  {
    _lib = dlopen(absPath, RTLD_LAZY);
    delete absPath;
  }
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
    _destroy = reinterpret_cast<decltype(_destroy)>(load("destroy"));
    _user1 = reinterpret_cast<decltype(_user1)>(load("user1"));
    _user2 = reinterpret_cast<decltype(_user2)>(load("user2"));
  }
}
FunKeyMonkeyModule::~FunKeyMonkeyModule()
{
  if(_lib)
    dlclose(_lib);
}
bool FunKeyMonkeyModule::ready() const
{
  return _lib != 0;
}
void FunKeyMonkeyModule::init(char const** argv, unsigned int argc)
{
  if(_init)
    (*_init)(argv, argc);
}
void FunKeyMonkeyModule::handle(input_event const& e, int src)
{
  if(_handle)
    (*_handle)(e, src);

}
void FunKeyMonkeyModule::destroy()
{
  if(_destroy)
    (*_destroy)();
}

void FunKeyMonkeyModule::user1()
{
  if(_user1)
    (*_user1)();
}

void FunKeyMonkeyModule::user2()
{
  if(_user2)
    (*_user2)();
}

#endif
