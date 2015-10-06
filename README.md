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

### Usage
<pre>
  ./funkeymonkey [OPTION...]

  -i, --device PATH         Input device to read from, eg. /dev/input/event0,
                            multiple can be provided
  -m, --match-devices PATH  Regular expression to match device strings
                            (format: '<vendor>,<product>,<version>,<name>') with,
                            matching will be read, multiple can be provided
  -p, --plugin PATH         Path to plugin
  -g, --grab                Grab the input device, preventing others from
                            accessing it
  -d, --daemonize           Daemonize process
  -l, --list-devices        List available devices
  -h, --help                Print help
</pre>

### Basics

FunKeyMonkey reads one or more evdev devices (basically any input device on a typical Linux setup) and relays their events to a plugin. Plugins are compiled separately and are provided to FunKeyMonkey on execution. 

A plugin often creates one or more virtual input devices using uinput. The plugin then typically reacts to the real input events and generates virtual ones based them.

When running FunKeyMonkey, you may decide to "grab" the input device(s) to prevent any other program from reading them. This way, only the virtual input is visible.

### Example use-cases

* Implementing an Fn-key on non-full keyboards
* Creating low-level keyboard macros
* Faking device-specific input, like integrated control devices for development
* Customizing keyboard controls for applications without built-in settings to do so

### General advice

List available input devices with `funkeymonkey -l`:
<pre>
<b># funkeymonkey -l</b>
/dev/input/event0: 0,1,0,Power Button
...
/dev/input/event14: 1235,ab12,110,NES30              NES30 Joystick
</pre>

You can test the desired device with with the libtestmodule.so to echo 
any input to the terminal:

<pre>
<b># sudo ./funkeymonkey -g -i /dev/input/event14 -p libtestmodule.so</b>
Successfully added device '/dev/input/event14'.
Init!
Event! 3 0 128 
...
</pre>

Using the `-g` option ensures no other applications can listen
directly to the chosen input device.

To match a device by metadata instead of a file path, use `-m`, for example `-m keyboard` would match any devices with "keyboard" in their name.

Notice you may need additional privileges in order to create uinput devices. Any modules that generate input events need this. Check your distribution documentation for details or run with root privileges. Your call.

To try if you have sufficient privileges for creating uinput devices, use `libtoymodule.so` with a keyboard (or other device that generates key events)  as an input device. It creates a virtual keyboard that inserts "k" key presses after every keyboard key up event. It's annoying but very easy to notice.

