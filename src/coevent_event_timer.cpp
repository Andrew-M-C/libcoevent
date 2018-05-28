
#include "coevent.h"
#include "coevent_itnl.h"
#include <string>

using namespace andrewmc::libcoevent;

typedef Event _super;

// ==========
// necessary definitions
#define __CO_EVENT_TIMER_DEFINITIONS
#ifdef __CO_EVENT_TIMER_DEFINITIONS

static int _g_libco_arg_counter = 0;    // to detect memory leaks


struct _EventArg {
    TimerEvent          *event;
    void                *user_arg;
    WorkerFunc          worker_func;
    struct stCoRoutine_t *coroutine;

    _EventArg() {
        _g_libco_arg_counter ++;
        DEBUG("Create evtimer arg, No %d", _g_libco_arg_counter);
    }

    ~_EventArg() {
        _g_libco_arg_counter --;
        DEBUG("Release evtimer arg, %d remain(s)", _g_libco_arg_counter);
    }
};


#endif


// ==========
// libco style routine, this routine is first part of the coroutine adapter
#define __CO_EVENT_TIMER_LIBCO_ROUTINE
#ifdef __CO_EVENT_TIMER_LIBCO_ROUTINE

static void *_libco_routine(void *libco_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libco_arg;
    (arg->worker_func)(-1, arg->event, arg->user_arg);
    return NULL;
}

#endif


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __CO_EVENT_TIMER_CALLBACK
#ifdef __CO_EVENT_TIMER_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;

    // switch into the coroutine
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the event if this is under control of the base
        TimerEvent *event = arg->event;
        Base  *base  = event->owner();

        DEBUG("evtimer %s ends", event->identifier().c_str());
        base->delete_event_under_control(event);
    }

    // done
    return;
}


#endif


// ==========
#define __PUBLIC_FUNCTIONS
#ifdef __PUBLIC_FUNCTIONS

TimerEvent::TimerEvent()
{
    DEBUG("Create timer event");
    this->_init();
    return;
}


TimerEvent::~TimerEvent()
{
    DEBUG("Delete timer event");
    this->_clear();
    return;
}


void TimerEvent::_init()
{
    _is_initialized = FALSE;

    char identifier[64];
    sprintf(identifier, "timer event %p", this);
    _identifier = identifier;

    return;
}


void TimerEvent::_clear()
{
    if (_event) {
        DEBUG("Delete evtimer");
        evtimer_del(_event);
        _event = NULL;
    }

    if (_owner_base) {
        DEBUG("clear owner base");
        _owner_base = NULL;
    }

    if (_event_arg) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        _event_arg = NULL;

        if (arg->coroutine) {
            DEBUG("remove coroutine");
            co_release(arg->coroutine);
            arg->coroutine = NULL;
        }

        DEBUG("Delete _event_arg");
        delete arg;
    }

    return;
}


struct Error TimerEvent::add_to_base(Base *base, WorkerFunc func, void *user_arg, BOOL auto_free)
{
    if (NULL == base) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    _clear();

    // create arg for libevent
    struct _EventArg *arg = new _EventArg;
    _event_arg = (void *)arg;
    arg->event = this;
    arg->user_arg = arg;
    arg->worker_func = func;

    // create arg for libco
    int call_ret = co_create(&(arg->coroutine), NULL, _libco_routine, arg);
    if (call_ret != 0) {
        _clear();
        _status.set_app_errno(ERR_LIBCO_CREATE);
        return _status;
    }

    // allocate a new evtimer
    _owner_base = base;
    _event = evtimer_new(base->event_base(), _libevent_callback, arg);
    if (NULL == _event) {
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }
    else {
        struct timeval sleep_time = {0, 0};
        evtimer_add(_event, &sleep_time);
    }

    // put event under control
    if (auto_free) {
        base->put_event_under_control(this);
    }

    _status.set_sys_errno(0);
    return _status;
}


TimerEvent::TimerEvent(Base *base, WorkerFunc func, void *arg)
{
    _init();
    add_to_base(base, func, arg);
    return;
}


void TimerEvent::sleep(double seconds)
{
    if (seconds <= 0) {
        return;
    }
    else {
        struct timeval sleep_time = to_timeval(seconds);
        struct _EventArg *arg = (struct _EventArg *)_event_arg;

        evtimer_add(_event, &sleep_time);
        co_yield(arg->coroutine);
    }
}


#endif  // end of __PUBLIC_FUNCTIONS

