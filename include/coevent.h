// file encoding: UTF-8

#ifndef __CO_EVENT_H__
#define __CO_EVENT_H__
#if defined (__cplusplus)
extern "C" {
#endif

#include "coevent_const.h"
#include "event2/event.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>

namespace andrewmc {
namespace libcoevent {

// libcoevent use this structure to return error information
struct CoEventError {
private:
    uint16_t _sys_errno;    // error defined in "errno.h"
    uint16_t _lib_errno;    // error defined by libcoevent in coevent_const.h
    std::string _err_msg;

public:
    CoEventError()
        _sys_errno(0),_lib_errno(0)
    {};

    void set_sys_errno(uint16_t sys_errno);
    void set_app_errno(uint16_t lib_errno, const char *c_err_msg = "");
    void set_app_errno(uint16_t lib_errno, const std::string &err_msg = "");
    const char *get_c_err_msg();
    uint32_t get_err_code();
};


// event base of libcoevent
class CoEventBase {
    // private implementations
private:
    struct event_base *_base;

    // constructor and destructors
public:
    CoEventBase()
    {
        self->_base = event_base_new();
        return;
    }

    ~CoEventBase(){};
}


// event of libcoevent
class CoEvent {
public:
    CoEvent(){};
    ~CoEvent(){};
};

}   // end of namespace libcoevent
}   // end of namespace andrewmc

#if defined (__cplusplus)
}
#endif
#endif  // EOF
