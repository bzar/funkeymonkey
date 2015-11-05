#include "funkeymonkeymodule.h"
#include "uinputdevice.h"
#include <iostream>

UinputDevice* out;
void init(char const** argv, unsigned int argc)
{
  std::cout << "Init!" << std::endl;
  out = new UinputDevice("/dev/uinput", BUS_USB, "FunKeyMonkey Toy", 1, 1, 1, {
    { EV_KEY, { KEY_K } }
  });
}
void handle(input_event const& e)
{
  std::cout << "Event! " << e.type << " " << e.code << " " << e.value << " " << std::endl;
  if(out && e.type == EV_KEY && e.value == 0)
  {
    out->send(EV_KEY, KEY_K, 1);
    out->send(EV_KEY, KEY_K, 0);
    out->send(EV_SYN, 0, 0);
  }
}
void destroy()
{
  std::cout << "Destroy!" << std::endl;
  if(out)
  {
    delete out;
  }
}

