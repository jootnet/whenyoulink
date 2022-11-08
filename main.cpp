#include "sciter-x.h"
#include "sciter-x-window.hpp"

#include <thread>

class frame: public sciter::window {
public:
  frame() : window(SW_TOOL | SW_MAIN) {}
};

#include "resources.cpp"

int uimain(std::function<int()> run ) {

  sciter::archive::instance().open(aux::elements_of(resources));

  sciter::om::hasset<frame> pwin = new frame();

  pwin->load( WSTR("this://app/main.htm") );
  
  pwin->expand();

  return run();
}
