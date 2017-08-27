#include "funkeymonkeymodule.h"
#include "uinputdevice.h"
#include <iostream>
#include <functional>
#include <array>

static constexpr unsigned int FIRST_KEY = KEY_RESERVED;
static constexpr unsigned int LAST_KEY = KEY_UNKNOWN;

template<int FIRST_KEY, int LAST_KEY>
class KeyBehaviors
{
public:
  void passthrough(unsigned int code);
  void map(unsigned int code, unsigned int result);
  void altmap(unsigned int code, bool* flag, 
              unsigned int regular, 
              unsigned int alternative);
  void complex(unsigned int code, std::function<void(int)> function);
  void handle(unsigned int code, int value);

private:
  struct KeyBehavior
  {
    KeyBehavior() : type(PASSTHROUGH), mapping(0), function() {}
    enum Type  { PASSTHROUGH, MAPPED, ALTMAPPED, COMPLEX };
    Type type;
    int mapping;
    int alternative;
    bool* flag;
    std::function<void(int)> function;
  };

  static constexpr unsigned int NUM_KEYS = LAST_KEY - FIRST_KEY + 1;
  std::array<KeyBehavior, NUM_KEYS> behaviors;
};

KeyBehaviors<FIRST_KEY, LAST_KEY>* behaviors;
UinputDevice* out;

void init(char const** argv, unsigned int argc)
{
  std::vector<unsigned int> keycodes;
  for(unsigned int i = FIRST_KEY; i <= LAST_KEY; ++i)
  {
    keycodes.push_back(i);
  }

  out = new UinputDevice("/dev/uinput", BUS_USB, "FunKeyMonkey keyboard", 1, 1, 1, {
    { EV_KEY, keycodes }
  });

  behaviors = new KeyBehaviors<FIRST_KEY, LAST_KEY>();

  // Example mappings
  behaviors->map(KEY_K, KEY_L);
  behaviors->complex(KEY_O, [](int value) {
    out->send(EV_KEY, KEY_I, value);
    out->send(EV_KEY, KEY_O, value);
  });
}
void handle(input_event const& e)
{
  if(e.type != EV_KEY)
    return;

  behaviors->handle(e.code, e.value);
  out->send(EV_SYN, 0, 0);
}
void destroy()
{
  if(out)
  {
    delete out;
  }
  if(behaviors)
  {
    delete behaviors;
  }
}
void user1()
{
}
void user2()
{
}


template<int FIRST_KEY, int LAST_KEY>
void KeyBehaviors<FIRST_KEY, LAST_KEY>::passthrough(unsigned int code)
{
  behaviors.at(code - FIRST_KEY).type = KeyBehavior::PASSTHROUGH;
}

template<int FIRST_KEY, int LAST_KEY>
void KeyBehaviors<FIRST_KEY, LAST_KEY>::map(unsigned int code, unsigned int result)
{
  auto& b = behaviors.at(code - FIRST_KEY);
  b.type = KeyBehavior::MAPPED;
  b.mapping = result;
}

template<int FIRST_KEY, int LAST_KEY>
void KeyBehaviors<FIRST_KEY, LAST_KEY>::altmap(unsigned int code, bool* flag,
    unsigned int regular, 
    unsigned int alternative)
{
  auto& b = behaviors.at(code - FIRST_KEY);
  b.type = KeyBehavior::ALTMAPPED;
  b.mapping = regular;
  b.alternative = alternative;
  b.flag = flag;
}

template<int FIRST_KEY, int LAST_KEY>
void KeyBehaviors<FIRST_KEY, LAST_KEY>::complex(unsigned int code, std::function<void(int)> function)
{
  auto& b = behaviors.at(code - FIRST_KEY);
  b.type = KeyBehavior::COMPLEX;
  b.function = function;
}

template<int FIRST_KEY, int LAST_KEY>
void KeyBehaviors<FIRST_KEY, LAST_KEY>::handle(unsigned int code, int value)
{
  KeyBehavior const& kb = behaviors.at(code - FIRST_KEY);
  switch(kb.type)
  {
    case KeyBehavior::PASSTHROUGH:
      out->send(EV_KEY, code, value);
      break;
    case KeyBehavior::MAPPED:
      out->send(EV_KEY, kb.mapping, value);
      break;
    case KeyBehavior::ALTMAPPED:
      out->send(EV_KEY, *kb.flag ? kb.alternative : kb.mapping, value);
      break; 
    case KeyBehavior::COMPLEX:
      kb.function(value);
      break;
  };

}
