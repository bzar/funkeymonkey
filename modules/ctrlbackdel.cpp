#include "funkeymonkeymodule.h"
#include "uinputdevice.h"
#include <iostream>

UinputDevice* out;
void init(char const** argv, unsigned int argc)
{
  // need to list all possible keys that we want to output in uinput
  std::vector<unsigned int> keycodes;
  for (unsigned int i = KEY_RESERVED; i <= KEY_UNKNOWN; ++i)
    keycodes.push_back(i);
  out = new UinputDevice("/dev/uinput", BUS_USB, "FunKeyCtrlBackDel", 1, 1, 1, {
    { EV_KEY, keycodes }
  });
}

void handle(input_event const& e, int)
{
  // all events will get handled, but we should technically only respond to the ones we made keys for!
  //std::cout << "\nEvent! " << e.type << " " << e.code << " " << e.value << "\n";
  if (e.type != EV_KEY)
  {
    if (e.type == EV_SYN)
        out->send(EV_SYN, e.code, e.value);
    return;
  }
  static bool left_ctrl_depressed = false;
  if (e.code == KEY_LEFTCTRL)
  {
    left_ctrl_depressed = e.value;
  }
  else if (e.code == KEY_BACKSPACE and left_ctrl_depressed)
  {
    // enforce LEFTCTRL is off when delete is pressed, otherwise strange things can happen 
    // (e.g. Ctrl+Delete means something different in the browser)
    if (e.value)
    {
      out->send(EV_KEY, KEY_LEFTCTRL, 0);
      out->send(EV_SYN, 0, 0);
      out->send(EV_KEY, KEY_DELETE, 1);
    }
    else
    {
      out->send(EV_KEY, KEY_DELETE, 0);
      out->send(EV_SYN, 0, 0);
      out->send(EV_KEY, KEY_LEFTCTRL, 1);
    }
    return;
  }
  // pass through everything else, including ctrl
  out->send(e.type, e.code, e.value);
}

void destroy()
{
  std::cout << "Destroy!" << std::endl;
  if (out)
  {
    delete out;
  }
}

void user1()
{
}
void user2()
{
}
