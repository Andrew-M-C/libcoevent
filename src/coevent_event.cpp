
#include "coevent.h"
#include "coevent_itnl.h"
#include <string>
#include <stdlib.h>

using namespace andrewmc::libcoevent;

static unsigned g_event_count = 0;

// ==========
#define __CO_EVENT_EVENT
#ifdef __CO_EVENT_EVENT

Event::Event()
{
    _owner_base = NULL;
    _event = NULL;
    _custom_storage = NULL;
    _custom_storage_size = 0;

    DEBUG("Create event %p, event count %u", this, ++g_event_count);
    return;
}


Event::~Event()
{
    if (_custom_storage)
    {
        free(_custom_storage);
        _custom_storage = NULL;
        _custom_storage_size = 0;
    }

    if (_event)
    {
        event_del(_event);
        _event = NULL;
    }

    DEBUG("Delete event %p, event count %u, (%s)", this, --g_event_count, this->identifier().c_str());
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


struct Error Event::create_custom_storage(size_t size)
{
    _status.clear_err();

    if (_custom_storage)
    {
        if (_custom_storage_size < size) {
            // should expand storage
            void *new_storage = realloc(_custom_storage, size);
            if (NULL == new_storage) {
                _status.set_sys_errno();
            }
            else {
                _custom_storage = new_storage;
                _custom_storage_size = size;
            }
        }
        else {
            // nothing to do
        }
    }
    else
    {
        // create a new storage
        _custom_storage = malloc(size);
        if (_custom_storage) {
            _custom_storage_size = size;
        }
        else {
            _status.set_sys_errno();
        }
    }

    // done
    return _status;
}


void *Event::custom_storage()
{
    return _custom_storage;
}


size_t Event::custom_storage_size()
{
    return _custom_storage_size;
}


#endif


// end of libcoevent::Event

// end of file
