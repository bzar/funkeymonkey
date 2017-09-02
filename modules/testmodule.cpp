#include "funkeymonkeymodule.h"
#include <iostream>
#include <dlfcn.h>

void (*_getName)(int src, char *name);

    void* load(std::string const& name) {
      void* value = dlsym(nullptr, name.data());
      auto error = dlerror();
      if(error)
      {
        std::cerr << "ERROR: While loading " << name << ": " << error << std::endl;
        value = nullptr;
      }
      return value;
    };


void init(char const** argv, unsigned int argc)
{
  std::cout << "Init!" << std::endl;
  for(unsigned int i = 0; i < argc; ++i)
  {
    std::cout << "Plugin parameter " << i + 1 << ": " << argv[i] << std::endl;
  }
  _getName = reinterpret_cast<decltype(_getName)>(load("getName"));
}
void handle(input_event const& e, int src)
{
	char tmp[256];
	_getName(src, tmp);
  std::cout << "Event! " << tmp << " " << e.type << " " << e.code << " " << e.value << " " << std::endl;
}
void destroy()
{
  std::cout << "Destroy!" << std::endl;
}

void user1()
{
  std::cout << "User1!" << std::endl;
}
void user2()
{
  std::cout << "User2!" << std::endl;
}
