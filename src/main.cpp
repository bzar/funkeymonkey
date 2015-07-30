#include "funkeymonkeymodule.h"
#include "evdevdevice.h"
#include "funkeymonkeymoduleloader.h"
#include "cxxopts.hpp"

#include <iostream>
#include <cstdlib>

#include <memory.h>
#include <signal.h>

volatile sig_atomic_t done = 0;

void term(int)
{
  done = 1;
}

int main(int argc, char** argv)
{
  cxxopts::Options options(argv[0], " - A evdev/uinput wrangler");
  options.add_options()
    ("i,device", "Input device to read from, eg. /dev/input/event0, multiple can be provided",
     cxxopts::value<std::vector<std::string>>(), "PATH")
    ("p,plugin", "Path to plugin", cxxopts::value<std::string>(), "PATH")
    ("g,grab", "Grab the input device, preventing others from accessing it")
    ("d,daemonize", "Daemonize process")
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

  if(options.count("i") < 1)
  {
    std::cerr << "ERROR: at least one input device is required" << std::endl;
    return EXIT_FAILURE;
  }

  if(options.count("p") != 1)
  {
    std::cerr << "ERROR: exactly one plugin is required" << std::endl;
    return EXIT_FAILURE;
  }

  EvdevDevice evdev(options["i"].as<std::vector<std::string>>());

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

  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = term;
  sigaction(SIGINT, &action, NULL);

  module.init();

  while(!done)
  {
    input_event e = evdev.poll();
    if(e.type != 0)
    {
      module.handle(e);
    }
  }

  module.destroy();

  return EXIT_SUCCESS;
}

