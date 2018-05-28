
#include "coevent.h"
#include "coevent_itnl.h"
#include <string>

using namespace andrewmc::libcoevent;

// ==========
#define __CO_EVENT_EVENT
#ifdef __CO_EVENT_EVENT

Event::Event()
{
    _owner_base = NULL;
    _event = NULL;

    DEBUG("Create event");
    return;
}


Event::~Event()
{
    DEBUG("Delete %s", this->identifier().c_str());
    return;
}


Base *Event::owner()
{
    return _owner_base;
}


void Event::set_identifier(std::string &identifier)
{
    _identifier = identifier;
    return;
}


const std::string &Event::identifier()
{
    return _identifier;
}


struct Error Event::status()
{
    return _status;
}


#endif  // end of libcoevent::Event

