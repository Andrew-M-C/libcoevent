
#include "coevent.h"
#include "coevent_itnl.h"
#include <string>
#include <stdio.h>
#include <set>

using namespace andrewmc::libcoevent;

// ==========
#define __CO_EVENT_BASE
#ifdef __CO_EVENT_BASE

Base::Base()
{
    _event_base = event_base_new();

    char identifier[64];
    sprintf(identifier, "licoevent base %p", this);
    _identifier = identifier;

    return;
}


Base::~Base()
{
    // free event base
    if (_event_base) {
        event_base_free(_event_base);
        _event_base = NULL;
    }

    // free all events under control
    for (std::set<Event *>::iterator it = _events_under_control.begin(); 
        it != _events_under_control.end(); 
        it ++)
    {
        delete *it;
    }
    _events_under_control.clear();

    return;
}


struct event_base *Base::event_base()
{
    return _event_base;
}


struct Error Base::run()
{
    struct Error ret_code;

    if (NULL == _event_base) {
        ret_code.set_app_errno(ERR_EVENT_BASE_NEW);
        return ret_code;
    }

    int err = event_base_dispatch(_event_base);
    if (0 == err) {
        // nothing more to do
    }
    else if (1 == err) {
        ret_code.set_app_errno(ERR_EVENT_BASE_NO_EVENT_PANDING);
    }
    else {
        ret_code.set_app_errno(ERR_EVENT_BASE_DISPATCH);
    }
    return ret_code;
}


void Base::set_identifier(std::string &identifier)
{
    _identifier = identifier;
    return;
}


const std::string &Base::identifier()
{
    return _identifier;
}


void Base::put_event_under_control(Event *event)
{
    _events_under_control.insert(event);
    return;
}


void Base::delete_event_under_control(Event *event)
{
    std::set<Event *>::iterator it = _events_under_control.find(event);
    if (it != _events_under_control.end()) {
        DEBUG("Delete event: %s", (*it)->identifier().c_str());
        delete *it;
        _events_under_control.erase(event);
    }
    else {
        DEBUG("Event %s is not under control", event->identifier().c_str());
    }
    return;
}


#endif  // end of libcoevent::Base

