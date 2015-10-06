#include "funkeymonkeymodule.h"
#include "uinputdevice.h"
#include <iostream>

/*
this code works for an 8bitdo SNES gamepad, 
with a nice mapping of buttons for the game CaveStory, e.g.:
start/select -> menu
    in menu:
        start -> resume play
        select -> restart game
A/B/X/Y/L/R do fairly intelligent things, but you can remap them below.

if you use a different gamepad, you may need to look for different event codes,
and your joystick may produce different event values for the various directions.

if you encounter any difficulties, test the controller out with libtestmodule.so
and replace the event values below to match your desired control scheme.

good luck!
*/


bool menu; // whether we're in the menu or not.

UinputDevice* out;
void init()
{
  std::cout << "Init!";
  menu = 0; // do not start on menu
  out = new UinputDevice("/dev/uinput", BUS_USB, "FunKeySNES", 1, 1, 1, {
    { EV_KEY, { KEY_Q, KEY_W, KEY_A, KEY_S, KEY_Z, KEY_X, KEY_UP, KEY_DOWN, KEY_RIGHT, KEY_LEFT, KEY_ESC, KEY_F1, KEY_F2 } }
  });
}
void handle(input_event const& e)
{
  std::cout << "\nEvent! " << e.type << " " << e.code << " " << e.value << " " << std::endl;
  if (e.type != 3) // not an x/y axis
  {
    unsigned int sendkey = 0;
    switch (e.code)
    {
    case 289: // A
      sendkey = KEY_W; // map
      break;
    case 290: // B
      sendkey = KEY_Z; // jump
      break;
    case 288: // X
      sendkey = KEY_Q; // inventory
      break;
    case 291: // Y
      sendkey = KEY_X; // shoot
      break;
    case 292: // L
      sendkey = KEY_A; // switch gun left
      break;
    case 293: // R
      sendkey = KEY_S; // switch gun right
      break;
    case 295: // start
      if (menu)
          sendkey = KEY_F1; // resume
      else
          sendkey = KEY_ESC; // pause
      if (!e.value) // toggling off
          menu = not menu;
      break;
    case 294: // select
      if (menu)
          sendkey = KEY_F2; // reset
      else
          sendkey = KEY_ESC; // pause
      if (!e.value) // toggling off
          menu = not menu;
      break;
    }
    if (out && sendkey)
    {
      if (e.value)
        out->send(EV_KEY, sendkey, 1);
      else
      {
        out->send(EV_KEY, sendkey, 0);
      }
      out->send(EV_SYN, 0, 0);
    }
  }
  else if (e.code == 0) // x axis 
  {
    if (e.value == 128) // off
    {
      out->send(EV_KEY, KEY_RIGHT, 0);
      out->send(EV_KEY, KEY_LEFT, 0);
      out->send(EV_SYN, 0, 0);
    }
    else if (e.value == 255) // right
    {
      out->send(EV_KEY, KEY_LEFT, 0);
      out->send(EV_KEY, KEY_RIGHT, 1);
      out->send(EV_SYN, 0, 0);
    }
    else // left
    {
      out->send(EV_KEY, KEY_RIGHT, 0);
      out->send(EV_KEY, KEY_LEFT, 1);
      out->send(EV_SYN, 0, 0);
    }
  }
  else // code == 1, y axis
  {
    if (e.value == 128) // off
    {
      out->send(EV_KEY, KEY_UP, 0);
      out->send(EV_KEY, KEY_DOWN, 0);
      out->send(EV_SYN, 0, 0);
    }
    else if (e.value == 255) // down
    {
      out->send(EV_KEY, KEY_UP, 0);
      out->send(EV_KEY, KEY_DOWN, 1);
      out->send(EV_SYN, 0, 0);
    }
    else // up
    {
      out->send(EV_KEY, KEY_DOWN, 0);
      out->send(EV_KEY, KEY_UP, 1);
      out->send(EV_SYN, 0, 0);
    }
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

