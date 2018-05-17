
#include "coevent.h"
#include <string>

using namespace andrewmc::libcoevent;

typedef Event _super;

// ==========
// necessary definitions
#define __CO_EVENT_TIMER_DEFINITIONS
#ifdef __CO_EVENT_TIMER_DEFINITIONS

static int _g_libco_arg_counter = 0;    // to detect memory leaks

typedef enum {
    UNKNOWN_TYPE = 0,
    STREAM_TYPE,
    DGRAM_TYPE,

    // TODO: 添加其他类型
} _EventType_t;


struct _LibcoArg {
    Event               *event;
    evutil_socket_t     fd;
    short               what;
    void                *user_arg;
    WorkerFunc          worker_func;

    _LibcoArg() {
        _g_libco_arg_counter ++;
    }

    ~_LibcoArg() {
        _g_libco_arg_counter --;
    }
};


#endif


// ==========
// libco style routine, this routine is first part of the coroutine adapter
#define __CO_EVENT_TIMER_LIBCO_ROUTINE
#ifdef __CO_EVENT_TIMER_LIBCO_ROUTINE

static void libco_routine(void *libco_arg)
{
    struct _LibcoArg *arg = (struct _LibcoArg *)arg;




    // TODO:
    return;
}

#endif


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __CO_EVENT_TIMER_CALLBACK
#ifdef __CO_EVENT_TIMER_CALLBACK

static void libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    Event *event = (Event *)libevent_arg;

    // TODO:
    return;
}


#endif


// ==========
#define __PUBLIC_FUNCTIONS
#ifdef __PUBLIC_FUNCTIONS

void TimerEvent::_init()
{
    _is_initialized = FALSE;

    char identifier[64];
    sprintf(identifier, "licoevent timer %p", this);
    _identifier = identifier;

    return;
}


TimerEvent::TimerEvent()
{
    this->_init();
    return;
}


struct Error TimerEvent::set_worker(Base *base, WorkerFunc func, void *arg)
{
    if (NULL == base) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    _owner_base = base;

    // TODO:

    _status.set_sys_errno(0);
    return _status;
}


TimerEvent::TimerEvent(Base *base, WorkerFunc func, void *arg)
{
    this->_init();
    this->set_worker(base, func, arg);
    return;
}


#endif  // end of __PUBLIC_FUNCTIONS

