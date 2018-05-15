// file encoding: UTF-8

#ifndef __CO_EVENT_H__
#define __CO_EVENT_H__

#include "coevent_const.h"
#include "event2/event.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <string>

namespace andrewmc {
namespace libcoevent {

// libcoevent use this structure to return error information
struct Error {
private:
    uint16_t _sys_errno;    // error defined in "errno.h"
    uint16_t _lib_errno;    // error defined by libcoevent in coevent_const.h
    std::string _err_msg;

public:
    Error():
        _sys_errno(0),_lib_errno(0)
    {}

    void set_sys_errno(int sys_errno);
    void set_app_errno(ErrCode_t lib_errno);
    void set_app_errno(ErrCode_t lib_errno, const char *c_err_msg);
    void set_app_errno(ErrCode_t lib_errno, const std::string &err_msg);
    const char *get_c_err_msg();
    uint32_t get_err_code();
};


// event base of libcoevent
class Base {
    // private implementations
private:
    struct event_base *_event_base;

    // constructor and destructors
public:
    Base()
    {
        _event_base = event_base_new();
        return;
    }

    ~Base()
    {
        if (_event_base) {
            event_base_free(_event_base);
            _event_base = NULL;
        }
        return;
    }

    struct Error run();
};


// event of libcoevent
class Event {
public:
    Event();
    ~Event();
};

}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
