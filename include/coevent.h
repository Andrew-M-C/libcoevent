// file encoding: UTF-8

#ifndef __CO_EVENT_H__
#define __CO_EVENT_H__

#include "coevent_const.h"
#include "event2/event.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <set>

namespace andrewmc {
namespace libcoevent {

class Base;
class Event;


// coroutine function
typedef void (*WorkerFunc)(evutil_socket_t, Event *, void *);


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

    BOOL is_error();
    BOOL is_ok();
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
    struct event_base   *_event_base;
    std::string         _identifier;
    std::set<Event *>   _events_under_control;  // User may put an event into a base, it will be deallocated automatically when this is not needed anymore.

    // constructor and destructors
public:
    Base();
    ~Base();
    struct event_base *event_base();
    struct Error run();
    void set_identifier(std::string &identifier);
    const std::string &identifier();
    void put_event_under_control(Event *event);
    void delete_event_under_control(Event *event);
};


// event of libcoevent
class Event {
protected:
    std::string     _identifier;
    Base            *_owner_base;
    struct event    *_event;
    struct Error    _status;
public:
    Event();
    virtual ~Event();
    Base *owner();
    void set_identifier(std::string &identifier);
    const std::string &identifier();
    struct Error status();
};


// pure event, no network interfaces supported
class TimerEvent : public Event {
protected:
    BOOL            _is_initialized;
    void            *_event_arg;
public:
    TimerEvent();
    TimerEvent(Base *base, WorkerFunc func, void *arg);
    ~TimerEvent();
    struct Error add_to_base(Base *base, WorkerFunc func, void *user_arg, BOOL auto_free = TRUE);
    void sleep(double seconds);     // can ONLY be incoked inside coroutine
private:
    void _init();
    void _clear();
};

}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
