
#include "coevent.h"
#include <string>

using namespace andrewmc::libcoevent;

// ==========
#define __CO_EVENT_EVENT
#ifdef __CO_EVENT_EVENT

Event::Event()
{
    _event = NULL;
    _owner_base = NULL;

    char identifier[64];
    sprintf(identifier, "licoevent event %p", this);
    _identifier = identifier;

    return;
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

#endif  // end of libcoevent::Base

