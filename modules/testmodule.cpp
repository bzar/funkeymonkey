#include "funkeymonkeymodule.h"
#include <iostream>

void init(char const** argv, unsigned int argc)
{
  std::cout << "Init!" << std::endl;
  for(unsigned int i = 0; i < argc; ++i)
  {
    std::cout << "Plugin parameter " << i + 1 << ": " << argv[i] << std::endl;
  }
}
void handle(input_event const& e, unsigned int role)
{
  std::cout << "Event from device with role " << role << ": " << e.type << " " << e.code << " " << e.value << " " << std::endl;
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
