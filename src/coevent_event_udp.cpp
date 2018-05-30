
#include "coevent.h"
#include "coevent_itnl.h"
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace andrewmc::libcoevent;

typedef Event _super;

// ==========
// necessary definitions
#define __CO_EVENT_UDP_DEFINITIONS
#ifdef __CO_EVENT_UDP_DEFINITIONS

struct _EventArg {
    UDPEvent            *event;
    int                 fd;
    struct stCoRoutine_t *coroutine;

    WorkerFunc          worker_func;
    void                *user_arg;
};


#endif


// ==========
// TODO:
// libco style routine, this routine is first part of the coroutine adapter
#define __CO_EVENT_UDP_LIBCO_ROUTINE
#ifdef __CO_EVENT_UDP_LIBCO_ROUTINE

static void *_libco_routine(void *libco_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libco_arg;
    (arg->worker_func)(-1, arg->event, arg->user_arg);
    return NULL;
}

#endif


// ==========
// TODO:
// libevent style callback, this callback is second part of the coroutine adapter
#define __CO_EVENT_UDP_CALLBACK
#ifdef __CO_EVENT_UDP_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;

    // switch into the coroutine
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the event if this is under control of the base
        UDPEvent *event = arg->event;
        Base  *base  = event->owner();

        DEBUG("evudp %s ends", event->identifier().c_str());
        base->delete_event_under_control(event);
    }

    // done
    return;
}


#endif


// ==========
#define __PUBLIC_INIT_AND_CLEAR_FUNCTIONS
#ifdef __PUBLIC_INIT_AND_CLEAR_FUNCTIONS

struct Error UDPEvent::init(Base *base, WorkerFunc func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg, BOOL auto_free)
{
    if (!(base && addr)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    if (addr->sa_family != AF_INET
        || addr->sa_family != AF_INET6
        || addr->sa_family != AF_UNIX)
    {
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }

    _clear();

    // create arguments
    struct _EventArg *arg = new _EventArg;
    _event_arg = arg;
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

    // determine network type
    if (AF_INET == addr->sa_family)
    {
        memcpy(&_sockaddr_ipv4, addr, sizeof(_sockaddr_ipv4));
        _fd_ipv4 = socket(AF_INET, SOCK_DGRAM, 0);
        if (_fd_ipv4 < 0) {
            _clear();
            _status.set_sys_errno(errno);
            return _status;
        }
    }
    else if (AF_INET6 == addr->sa_family)
    {
        memcpy(&_sockaddr_ipv6, addr, sizeof(_sockaddr_ipv6));
        _fd_ipv6 = socket(AF_INET6, SOCK_DGRAM, 0);
        if (_fd_ipv6 < 0) {
            _clear();
            _status.set_sys_errno(errno);
            return _status;
        }
    }
    else
    {
        memcpy(&_sockaddr_unix, addr, sizeof(_sockaddr_unix));
        _fd_unix = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (_fd_unix < 0) {
            _clear();
            _status.set_sys_errno(errno);
            return _status;
        }
    }

    // try binding
    int fd = _fd_ipv4 ? _fd_ipv4 : (_fd_ipv6 ? _fd_ipv4 : _fd_unix);
    int status = bind(_fd_ipv4, addr, addr_len);
    if (status < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // non-block
    set_fd_nonblock(fd);

    // create event
    _owner_base = base;
    _event = event_new(base->event_base(), fd, EV_TIMEOUT | EV_READ | EV_ET, _libevent_callback, user_arg);
    if (NULL == _event) {
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }
    else {
        struct timeval sleep_time = {0, 0};
        event_add(_event, &sleep_time);
    }

    // automatic free
    if (auto_free) {
        base->put_event_under_control(this);
    }

    _status.clear_err();
    return _status;
}


void UDPEvent::_init()
{
    char identifier[64];
    sprintf(identifier, "UDP event %p", this);
    _identifier = identifier;
    return;
}


void UDPEvent::_clear()
{
    if (_event) {
        DEBUG("Delete io event");
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

    if (_fd_ipv4) {
        close(_fd_ipv4);
    }
    if (_fd_ipv6) {
        close(_fd_ipv6);
    }
    if (_fd_unix) {
        close(_fd_unix);
    }

    _event_arg = NULL;
    _fd_ipv4 = 0;
    _fd_ipv6 = 0;
    _fd_unix = 0;
    return;
}

#endif  // end of __PUBLIC_INIT_AND_CLEAR_FUNCTIONS


// ==========
#define __PUBLIC_MISC_FUNCTIONS
#ifdef __PUBLIC_MISC_FUNCTIONS



#endif  // end of __PUBLIC_MISC_FUNCTIONS

