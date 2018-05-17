
#include "coevent.h"
#include <string>
#include <stdio.h>

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
    if (_event_base) {
        event_base_free(_event_base);
        _event_base = NULL;
    }
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

#endif  // end of libcoevent::Base

