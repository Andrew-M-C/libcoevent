
#include "coevent.h"
#include "coevent_itnl.h"
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <map>

using namespace andrewmc::libcoevent;

// ==========
// necessary definitions
#define __CO_EVENT_TCP_SESSION_ARGUMENTS
#ifdef __CO_EVENT_TCP_SESSION_ARGUMENTS

struct _EventArg {
    TCPItnlSession      *session;
    int                 fd;
    uint32_t            *libevent_what_ptr;

    struct stCoRoutine_t *coroutine;
    WorkerFunc          worker_func;
    void                *user_arg;
};

#endif  // end of __CO_EVENT_TCP_SESSION_ARGUMENTS



// ==========
// libco style routine, this routine is first part of the coroutine adapter
#define __CO_EVENT_TCP_SESSION_LIBCO_ROUTINE
#ifdef __CO_EVENT_TCP_SESSION_LIBCO_ROUTINE

static void *_libco_routine(void *libco_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libco_arg;
    (arg->worker_func)(arg->fd, arg->session, arg->user_arg);
    return NULL;
}

#endif  // end of __CO_EVENT_TCP_SESSION_LIBCO_ROUTINE


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __CO_EVENT_UDP_CALLBACK
#ifdef __CO_EVENT_UDP_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;

    // switch into the coroutine
    if (arg->libevent_what_ptr) {
        *(arg->libevent_what_ptr) = (uint32_t)what;
        DEBUG("libevent what: 0x%04x - %s%s", (unsigned)what, event_is_timeout(what) ? "timeout " : "", event_readable(what) ? "read" : "");
    }

    // handle control to user application
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine))
    {
        // delete the event if this is under control of the base
        TCPItnlSession *session = arg->session;
        TCPServer *server = session->server();
        Base *base = session->owner();

        DEBUG("evtcp %s ends", session->identifier().c_str());
        server->notify_session_ends(session);
        base->delete_event_under_control(session);
    }

    // done
    return;
}

#endif


// ==========
#define __CONSTRUCT_AND_DESTRUCTORS
#ifdef __CONSTRUCT_AND_DESTRUCTORS

TCPItnlSession::TCPItnlSession()
{
    char identifier[64];
    sprintf(identifier, "TCP session %p", this);
    _identifier = identifier;

    _fd = 0;
    _remote_addr.ss_family = (sa_family_t)0;
    _addr_len = 0;
    _server = NULL;
    _event_arg = NULL;

    _libevent_what_storage = (uint32_t *)malloc(sizeof(*_libevent_what_storage));
    if (NULL == _libevent_what_storage) {
        _status.set_sys_errno();
        throw std::bad_alloc();
        return;
    }

    return;
}


TCPItnlSession::~TCPItnlSession()
{
    _clear();

    if (_libevent_what_storage) {
        free(_libevent_what_storage);
        _libevent_what_storage = NULL;
    }
    return;
}


void TCPItnlSession::_clear()
{
    struct _EventArg *arg = (struct _EventArg *)_event_arg;
    if (arg) {
        if (arg->coroutine) {
            co_release(arg->coroutine);
        }
        free(arg);
        _event_arg = NULL;
    }

    if (_event) {
        event_del(_event);
        _event = NULL;
    }

    if (_fd > 0) {
        close(_fd);
        _fd = 0;
    }

    _remote_addr.ss_family = (sa_family_t)0;
    _addr_len = 0;
    _server = NULL;
    _event_arg = NULL;
    return;
}


#endif  // end of __CONSTRUCT_AND_DESTRUCTORS


// ==========
#define __INIT_FUNTIONS
#ifdef __INIT_FUNTIONS

struct Error TCPItnlSession::init(TCPServer *server, int fd, WorkerFunc func, const struct sockaddr *remote_addr, socklen_t addr_len, void *user_arg)
{
    if (!(server && fd > 0 && func && remote_addr && addr_len)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    _clear();
    _status.clear_err();

    switch (remote_addr->sa_family)
    {
        case AF_INET:
            _addr_len = sizeof(struct sockaddr_in);
            break;
        case AF_INET6:
            _addr_len = sizeof(struct sockaddr_in6);
            break;
        case AF_UNIX:
            _addr_len = sizeof(struct sockaddr_un);
            break;
        default:
            _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
            return _status;
            break;
    }

    if (addr_len < _addr_len) {
        ERROR("addr len is %u, should be %u", (unsigned)addr_len, (unsigned)_addr_len);
        _addr_len = 0;
        _status.set_app_errno(ERR_PARA_ILLEGAL);
        return _status;
    }

    // create arguments
    struct _EventArg *arg = (struct _EventArg *)malloc(sizeof(*arg));
    if (NULL == arg) {
        throw std::bad_alloc();
        _status.set_sys_errno();
        return _status;
    }

    _event_arg = arg;
    arg->session = this;
    arg->libevent_what_ptr = _libevent_what_storage;
    arg->worker_func = func;
    arg->user_arg = user_arg;
    arg->coroutine = NULL;
    memcpy(&_remote_addr, remote_addr, _addr_len);

    // create routine for libco
    int call_ret = co_create(&(arg->coroutine), NULL, _libco_routine, arg);
    if (call_ret != 0) {
        _clear();
        _status.set_app_errno(ERR_LIBCO_CREATE);
        return _status;
    }

    // non block
    _fd = fd;
    set_fd_nonblock(fd);

    // create event
    _server = server;
    _owner_base = server->owner();
    _event = event_new(_owner_base->event_base(), fd, EV_TIMEOUT | EV_READ, _libevent_callback, arg);
    if (NULL == _event) {
        ERROR("Failed to new a UDP event");
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }
    else {
        struct timeval sleep_time = {0, 0};
        DEBUG("Add UDP event %s", _identifier.c_str());
        event_add(_event, &sleep_time);
    }

    // auto_free
    _owner_base->put_event_under_control(this);

    // done
    return _status;
}

#endif  // end of __INIT_FUNTIONS


// ==========
#define __SEND_FUNCTION
#ifdef __SEND_FUNCTION

struct Error TCPItnlSession::reply(const void *data, const size_t data_len, size_t *send_len_out_nullable)
{
    ssize_t send_len = 0;

    if (!(data && data_len)) {
        _status.set_app_errno(ERR_PARA_NULL);
    }
    else if (_fd <= 0) {
        _status.set_app_errno(ERR_NOT_INITIALIZED);
    }
    else {
        send_len = write(_fd, data, data_len);
        if (send_len < 0) {
            _status.set_sys_errno();
        }
        else {
            _status.clear_err();
        }
    }

    if (send_len_out_nullable) {
        *send_len_out_nullable = (send_len > 0) ? send_len : 0;
    }
    return _status;
}


#endif  // end of __SEND_FUNCTION


// ==========
#define __RECV_FUNCTION
#ifdef __RECV_FUNCTION


struct Error TCPItnlSession::recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out, const struct timeval &timeout)
{
    ssize_t recv_len = 0;
    struct _EventArg *arg = (struct _EventArg *)_event_arg;
    volatile uint32_t libevent_what = 0;

    // param check
    if (NULL == data_out) {
        ERROR("no recv data buffer spectied");
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }
    if (_fd <= 0 || NULL == _libevent_what_storage) {
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return _status;
    }
    _status.clear_err();

    // recvfrom()
    libevent_what = *_libevent_what_storage;
    if (event_readable(libevent_what))
    {
        // data readable
        recv_len = read(_fd, data_out, len_limit);
        if (recv_len < 0) {
            _status.set_sys_errno();
        }
        // EAGAIN
        else if (0 == recv_len) {
            DEBUG("EAGAIN");
            *_libevent_what_storage &= ~EV_READ;
            return recv_in_timeval(data_out, len_limit, len_out, timeout);
        }
    }
    else {
        // no data avaliable
        struct timeval clock_now = ::andrewmc::cpptools::sys_up_timeval();
        struct timeval clock_timeout;
        struct timeval timeout_copy;
        timeout_copy.tv_sec = timeout.tv_sec;
        timeout_copy.tv_usec = timeout.tv_usec;

        DEBUG("TCP libevent what flag: 0x%04x, now wait", (unsigned)(*_libevent_what_storage));
        if ((0 == timeout_copy.tv_sec) && (0 == timeout_copy.tv_usec)) {
            timeout_copy.tv_sec = FOREVER_SECONDS;
            clock_timeout.tv_sec = FOREVER_SECONDS;
        }
        else {
            timeradd(&clock_now, &timeout_copy, &clock_timeout);
        }
        *_libevent_what_storage &= ~EV_TIMEOUT;
        event_add(_event, &timeout_copy);
        co_yield(arg->coroutine);

        // check if timeout
        libevent_what = *_libevent_what_storage;
        if (event_is_timeout(libevent_what)) {
            _status.set_app_errno(ERR_TIMEOUT);
        }
        else if (event_readable(libevent_what)) {
            recv_len = read(_fd, data_out, len_limit);
            if (recv_len < 0) {
                _status.set_sys_errno();
            }
        }
        else {
            ERROR("unrecognized event flag: 0x%04u", libevent_what);
            _status.set_app_errno(ERR_UNKNOWN);
        }
    }

    // write read data len and return
    if (len_out) {
        *len_out = (recv_len > 0) ? recv_len : 0;
    }
    return _status;
}


struct Error TCPItnlSession::recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out, unsigned mili_secs)
{
    struct timeval timeout = to_timeval_from_milisecs(mili_secs);
    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}


struct Error TCPItnlSession::recv(void *data_out, const size_t len_limit, size_t *len_out, double timeout_seconds)
{
    struct timeval timeout = {0, 0};
    if (timeout_seconds > 0) {
        timeout = to_timeval(timeout_seconds);
    }

    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}


#endif  // end of __RECV_FUNCTION


// ==========
#define __SLEEP_FUNCTIONS
#ifdef __SLEEP_FUNCTIONS


struct Error TCPItnlSession::sleep(struct timeval &sleep_time)
{
    struct _EventArg *arg = (struct _EventArg *)_event_arg;

    _status.clear_err();
    if ((0 == sleep_time.tv_sec) && (0 == sleep_time.tv_usec)) {
        return _status;
    }

    event_add(_event, &sleep_time);
    co_yield(arg->coroutine);

    // determine libevent event masks
    uint32_t libevent_what = (_libevent_what_storage) ? *_libevent_what_storage : 0;
    if (event_is_timeout(libevent_what))
    {
        // normally timeout
        _status.clear_err();
        return _status;
    }
    else if (event_readable(libevent_what))
    {
        // read event occurred
        _status.set_app_errno(ERR_INTERRUPTED_SLEEP);
        return _status;
    }
    else {
        // unexpected error
        ERROR("%s - unexpected libevent masks: 0x%04x", identifier().c_str(), (unsigned)libevent_what);
        _status.set_app_errno(ERR_UNKNOWN);
        return _status;
    }
}


struct Error TCPItnlSession::sleep_milisecs(unsigned mili_secs)
{
    struct timeval timeout = to_timeval_from_milisecs(mili_secs);
    return sleep(timeout);
}


struct Error TCPItnlSession::sleep(double seconds)
{
    if (seconds <= 0) {
        _status.clear_err();
        return _status;
    }
    else {
        struct timeval timeout = to_timeval(seconds);
        return sleep(timeout);
    }
}

#endif  // end of __SLEEP_FUNCTIONS


// ==========
#define __MISC_FUNCTIONS
#ifdef __MISC_FUNCTIONS

TCPServer *TCPItnlSession::server()
{
    return _server;
}


int TCPItnlSession::file_descriptor()
{
    return _fd;
}


NetType_t TCPItnlSession::network_type()
{
    switch(_remote_addr.ss_family)
    {
        case AF_UNIX:
            return NetLocal;
            break;
        case AF_INET:
            return NetIPv4;
            break;
        case AF_INET6:
            return NetIPv6;
            break;
        default:
            return NetUnknown;
            break;
    }
    return NetUnknown;
}


std::string TCPItnlSession::remote_addr()
{
    if (AF_INET == _remote_addr.ss_family) {
        struct sockaddr_in *addr = (struct sockaddr_in *)&_remote_addr;
        return str_from_sin_addr(&(addr->sin_addr));
    }
    else if (AF_INET6 == _remote_addr.ss_family) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&_remote_addr;
        return str_from_sin6_addr(&(addr6->sin6_addr));
    }
    else {
        return "";
    }
}


unsigned TCPItnlSession::remote_port()
{
    if (AF_INET == _remote_addr.ss_family) {
        return (unsigned)ntohs(((struct sockaddr_in *)&_remote_addr)->sin_port);
    }
    else if (AF_INET6 == _remote_addr.ss_family) {
        return (unsigned)ntohs(((struct sockaddr_in6 *)&_remote_addr)->sin6_port);
    }
    else {
        return 0;
    }
}


void TCPItnlSession::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    if (addr_out && addr_len <= _addr_len) {
        memcpy((void *)addr_out, &_remote_addr, addr_len);
    }
    return;
}


#endif  // end of __MISC_FUNCTIONS

