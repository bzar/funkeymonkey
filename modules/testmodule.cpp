#include "funkeymonkeymodule.h"
#include <iostream>

void init()
{
  std::cout << "Init!" << std::endl;
}
void handle(input_event const& e)
{
  std::cout << "Event! " << e.type << " " << e.code << " " << e.value << " " << std::endl;
}
void destroy()
{
  std::cout << "Destroy!" << std::endl;
}

