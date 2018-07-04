
#include "coevent.h"
#include "coevent_itnl.h"
#include <string>

using namespace andrewmc::libcoevent;

typedef Server _super;

// ==========
// necessary definitions
#define __CO_EVENT_NO_SERVER_DEFINITIONS
#ifdef __CO_EVENT_NO_SERVER_DEFINITIONS

static int _g_libco_arg_counter = 0;    // to detect memory leaks


struct _EventArg {
    NoServer            *event;
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
#define __CO_EVENT_NO_SERVER_LIBCO_ROUTINE
#ifdef __CO_EVENT_NO_SERVER_LIBCO_ROUTINE

static void *_libco_routine(void *libco_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libco_arg;
    (arg->worker_func)(-1, arg->event, arg->user_arg);
    return NULL;
}

#endif


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __CO_EVENT_NO_SERVER_CALLBACK
#ifdef __CO_EVENT_NO_SERVER_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;

    // switch into the coroutine
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the server if this is under control of the base
        NoServer *server = arg->event;
        Base *base = server->owner();

        DEBUG("evtimer %s ends", server->identifier().c_str());
        base->delete_event_under_control(server);
    }

    // done
    return;
}


#endif


// ==========
#define __PUBLIC_FUNCTIONS
#ifdef __PUBLIC_FUNCTIONS

NoServer::NoServer()
{
    DEBUG("Create 'NO' server");
    this->_init();
    return;
}


NoServer::~NoServer()
{
    DEBUG("Delete 'NO' server");
    this->_clear();
    return;
}


void NoServer::_init()
{
    char identifier[64];
    sprintf(identifier, "NO server %p", this);
    _identifier = identifier;

    _event_arg = NULL;
    return;
}


void NoServer::_clear()
{
    if (_event) {
        DEBUG("Delete evtimer");
        evtimer_del(_event);
        _event = NULL;
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


struct stCoRoutine_t *NoServer::_coroutine()
{
    if (_event_arg) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        return arg->coroutine;
    }
    else {
        return NULL;
    }
}


struct Error NoServer::init(Base *base, WorkerFunc func, void *user_arg, BOOL auto_free)
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
    arg->user_arg = user_arg;
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


struct Error NoServer::sleep(const struct timeval &sleep_time)
{
    struct _EventArg *arg = (struct _EventArg *)_event_arg;
    struct timeval sleep_time_copy;
    sleep_time_copy.tv_sec = sleep_time.tv_sec;
    sleep_time_copy.tv_usec = sleep_time.tv_usec;

    evtimer_add(_event, &sleep_time_copy);
    co_yield(arg->coroutine);

    _status.clear_err();
    return _status;
}


struct Error NoServer::sleep_milisecs(unsigned mili_secs)
{
    return sleep(to_timeval_from_milisecs(mili_secs));
}


struct Error NoServer::sleep(double seconds)
{
    if (seconds <= 0) {
        _status.clear_err();
        return _status;
    }
    else {
        struct timeval sleep_time = to_timeval(seconds);
        return sleep(sleep_time);
    }
}


#endif  // end of __PUBLIC_FUNCTIONS

