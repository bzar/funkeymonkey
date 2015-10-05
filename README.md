# FunKeyMonkey

An efficient evdev/uinput wrangler with a plugin architecture.


### Install

Standard CMake stuff:

```
mkdir build
cd build
cmake ..
make
```


### Example usage

List available input devices with `funkeymonkey -l`:
```
# funkeymonkey -l
/dev/input/event0: 0,1,0,Power Button
...
/dev/input/event14: 1235,ab12,110,NES30              NES30 Joystick
```

You can test the desired device with with the libtoymodule.so to echo 
any input to the terminal:

```
# sudo ./funkeymonkey -g -i /dev/input/event14 -p libtoymodule.so
Successfully added device '/dev/input/event14'.
Init!
Event! 3 0 128 
...
```

Using the `-g` option ensures no other applications can listen
directly to the chosen input device.

Notice you may need root privileges in order to send keycodes to
windows outside this terminal.

