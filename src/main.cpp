#include "funkeymonkeymodule.h"
#include "evdevdevice.h"
#include "funkeymonkeymoduleloader.h"
#include "cxxopts.hpp"

#include <iostream>
#include <cstdlib>
#include <regex>
#include <algorithm>
#include <iterator>

#include <memory.h>
#include <signal.h>

volatile sig_atomic_t done = 0;
volatile sig_atomic_t usr1 = 0;
volatile sig_atomic_t usr2 = 0;

EvdevDevice *p_evdev;

void sighandle(int sig)
{
  switch(sig)
  {
    case SIGINT:
      done = 1;
      break;
    case SIGUSR1:
      usr1 = 1;
      break;
    case SIGUSR2:
      usr2 = 1;
      break;
    default: break;
  }
}

void process(EvdevDevice& evdev, FunKeyMonkeyModule& module,
    std::vector<std::string> const& moduleArgs)
{
  struct sigaction sigAction;
  sigAction.sa_handler = sighandle;
  sigAction.sa_flags = 0;
  sigemptyset(&sigAction.sa_mask);

  if(sigaction(SIGINT, &sigAction, NULL) == -1
      || sigaction(SIGUSR1, &sigAction, NULL) == -1
      || sigaction(SIGUSR2, &sigAction, NULL) == -1)
  {
    std::cerr << "ERROR: Could not register signal handlers" << std::endl;
  }

  std::vector<char const*> args;
  std::transform(moduleArgs.begin(), moduleArgs.end(),
      std::inserter(args, args.begin()), [](std::string const& s) {
      return s.c_str();
  });

  module.init(args.data(), args.size());

  while(!done)
  {
    if(usr1)
    {
      module.user1();
      usr1 = 0;
    }

    if(usr2)
    {
      module.user2();
      usr2 = 0;
    }

    auto result = evdev.poll();
    switch(result.status)
    {
      case EvdevDevice::POLL_OK:
      {
        module.handle(result.event, evdev.getLastfd());
        break;
      }
      case EvdevDevice::POLL_TIMEOUT:
      {
        break;
      }
      case EvdevDevice::POLL_ERROR:
      {
        if(!done && !usr1 && !usr2)
        {
          std::cerr << "ERROR: Reading devices failed." << std::endl;
          done = 1;
        }
        break;
      }
    }
  }

  module.destroy();
}

void getName(int src, char *name) {
	strcpy(name, p_evdev->getName(src).data());
}

int main(int argc, char** argv)
{
  cxxopts::Options options(argv[0], " - A evdev/uinput wrangler");
  options.add_options()
    ("i,device", "Input device to read from, eg. /dev/input/event0, multiple can be provided",
     cxxopts::value<std::vector<std::string>>(), "PATH")
    ("m,match-devices", "Regular expression to match device strings (format: '<vendor>,<product>,<version>,<name>') with, matching will be read, multiple can be provided",
     cxxopts::value<std::vector<std::string>>(), "PATTERN")
    ("p,plugin", "Path to plugin", cxxopts::value<std::string>(), "PATH")
    ("g,grab", "Grab the input device, preventing others from accessing it")
    ("d,daemonize", "Daemonize process")
    ("l,list-devices", "List available devices")
    ("X,plugin-parameter", "Plugin parameter",
     cxxopts::value<std::vector<std::string>>(), "ARG")
    ("h,help", "Print help");

  if(argc == 1)
  {
    std::cout << options.help({"", "Group"}) << std::endl;
    return EXIT_SUCCESS;
  }

  try
  {
    options.parse(argc, argv);
  }
  catch(cxxopts::OptionException e)
  {
    std::cerr << "ERROR: parsing options failed: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  if(options.count("h"))
  {
    std::cout << options.help({"", "Group"}) << std::endl;
    return EXIT_SUCCESS;
  }

  if(options.count("l"))
  {
    std::vector<EvdevDevice::Information> devices = EvdevDevice::availableDevices();
    for(EvdevDevice::Information const& device : devices)
    {
      std::cout << std::hex
        << device.path << ": "
        << device.vendor << ","
        << device.product << ","
        << device.version << ","
        << device.name << std::endl;
    }
    return EXIT_SUCCESS;
  }

  if(options.count("i") < 1 && options.count("m") < 1)
  {
    std::cerr << "ERROR: at least one input device path or match pattern is required" << std::endl;
    return EXIT_FAILURE;
  }

  if(options.count("p") != 1)
  {
    std::cerr << "ERROR: exactly one plugin is required" << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<std::string> devicePaths = options["i"].as<std::vector<std::string>>();

  if(options.count("m") > 0)
  {
    std::vector<EvdevDevice::Information> devices = EvdevDevice::availableDevices();
    for(std::string const& pattern : options["m"].as<std::vector<std::string>>())
    {
      std::regex re(pattern);
      for(EvdevDevice::Information const& info : devices)
      {
        std::ostringstream deviceString;
        deviceString << std::hex
          << info.vendor << ","
          << info.product << ","
          << info.version << ","
          << info.name;
        if(std::regex_search(deviceString.str(), re))
        {
          devicePaths.push_back(info.path);
        }
      }
    }
  }

  EvdevDevice evdev(devicePaths);
  p_evdev  = &evdev;

  if(!evdev.ready())
  {
    std::cerr << "ERROR: Could not open input device" << std::endl;
    return EXIT_FAILURE;
  }

  FunKeyMonkeyModule module(options["p"].as<std::string>());

  if(!module.ready())
  {
    std::cerr << "ERROR: Could not open plugin" << std::endl;
    return EXIT_FAILURE;
  }

  if(options.count("g") && !evdev.grab(true))
  {
    std::cerr << "ERROR: Could not grab input device" << std::endl;
    return EXIT_FAILURE;
  }

  if(options.count("d") && daemon(1, 0) == -1)
  {
    std::cerr << "ERROR: Could not daemonize, error code " << errno << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<std::string> moduleArgs = options["X"].as<std::vector<std::string>>();

  process(evdev, module, moduleArgs);

  return EXIT_SUCCESS;
}

