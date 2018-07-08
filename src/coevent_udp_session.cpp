
#include "coevent.h"
#include "coevent_itnl.h"
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

using namespace andrewmc::libcoevent;

// ==========
// necessary definitions
#define __CO_EVENT_UDP_DEFINITIONS
#ifdef __CO_EVENT_UDP_DEFINITIONS

struct _EventArg {
    UDPSession          *event;
    int                 fd;
    uint32_t            *libevent_what_ptr;

    struct stCoRoutine_t *coroutine;
    WorkerFunc          worker_func;
    void                *user_arg;

    _EventArg(): event(NULL), fd(0), libevent_what_ptr(NULL), coroutine(NULL)
    {}
};

#endif


// ==========
// libco style routine, this routine is first part of the coroutine adapter
#define __CO_EVENT_UDP_LIBCO_ROUTINE
#ifdef __CO_EVENT_UDP_LIBCO_ROUTINE

static void *_libco_routine(void *libco_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libco_arg;
    (arg->worker_func)(arg->fd, arg->event, arg->user_arg);
    return NULL;
}

#endif


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
        DEBUG("libevent what: 0x%08x - %s%s", (unsigned)what, event_is_timeout(what) ? "timeout " : "", event_readable(what) ? "read" : "");
    }
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the event if this is under control of the base
        UDPSession *event = arg->event;
        Base *base  = event->owner();

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

UDPItnlSession::UDPItnlSession()
{
    char identifier[64];
    sprintf(identifier, "UDP session %p", this);
    _identifier = identifier;

    _event_arg = NULL;
    _fd = 0;
    _remote_addr_len = 0;
    _port = 0;
    _remote_addr.ss_family = 0;
    _self_addr.ss_family = 0;
    _data_buff.clear();
    _data_offset = 0;
    _data_len_to_read = 0;

    _libevent_what_storage = (uint32_t *)malloc(sizeof(*_libevent_what_storage));
    if (NULL == _libevent_what_storage) {
        throw std::bad_alloc();
    }
    else {
        *_libevent_what_storage = 0;
    }
    return;
}


UDPItnlSession::~UDPItnlSession()
{
    _clear();

    if (_libevent_what_storage) {
        free(_libevent_what_storage);
        _libevent_what_storage = NULL;
    }
    return;
}


void UDPItnlSession::_clear()
{
    if (_event) {
        DEBUG("Delete IO event");
        event_del(_event);
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
        free(arg);
    }

    if (_fd > 0) {
        close(_fd);
    }
    _fd = 0;
    _server_fd = 0;
    return;
}



struct Error UDPItnlSession::init(Base *base, int server_fd, WorkerFunc func, const struct sockaddr *remote_addr, socklen_t addr_len, void *user_arg)
{
    const BOOL auto_free = TRUE;

    if (!(base && remote_addr && server_fd > 0)) {
        DEBUG("%p, %p, %u", base, remote_addr, (unsigned)server_fd);
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    if (remote_addr->sa_family != AF_INET
        && remote_addr->sa_family != AF_INET6)
    {
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }

    _clear();
    _server_fd = server_fd;

    // create arguments
    struct _EventArg *arg = (struct _EventArg *)malloc(sizeof(*arg));
    if (NULL == arg) {
        throw std::bad_alloc();
        _status.set_sys_errno();
        return _status;
    }
    _event_arg = arg;
    arg->event = this;
    arg->user_arg = user_arg;
    arg->worker_func = func;
    arg->libevent_what_ptr = _libevent_what_storage;
    DEBUG("arg->libevent_what_ptr = %p", arg->libevent_what_ptr);
    DEBUG("User arg: %08p", user_arg);

    // create arg for libco
    DEBUG("Now test malloc");
    {
        void * temp_buff = malloc(64);
        if (temp_buff) {
            free(temp_buff);
            DEBUG("Test OK");
        }
    }
    int call_ret = co_create(&(arg->coroutine), NULL, _libco_routine, arg);
    if (call_ret != 0) {
        _clear();
        _status.set_app_errno(ERR_LIBCO_CREATE);
        return _status;
    }
    else {
        DEBUG("Init coroutine %p", arg->coroutine);
    }

    // determine network type
    struct sockaddr_storage sock_addr;
    memcpy(&_remote_addr, remote_addr, addr_len);
    if (AF_INET == remote_addr->sa_family)
    {
        struct sockaddr_in *addr = (struct sockaddr_in *)&sock_addr;
        _remote_addr_len = sizeof(*addr);
        addr->sin_family = AF_INET;
        addr->sin_port = htons(((struct sockaddr_in *)remote_addr)->sin_port);
        addr->sin_addr.s_addr = htonl(INADDR_ANY);

        addr = (struct sockaddr_in *)&_self_addr;
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        _fd = socket(AF_INET, SOCK_DGRAM, 0);
    }
    else if (AF_INET6 == remote_addr->sa_family)
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&sock_addr;
        _remote_addr_len = sizeof(*addr6);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(((struct sockaddr_in *)remote_addr)->sin_port);
        addr6->sin6_addr = in6addr_any;

        addr6 = (struct sockaddr_in6 *)&_self_addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_loopback;

        _fd = socket(AF_INET6, SOCK_DGRAM, 0);
    }
    else
    {
        ERROR("Invalid socket type %u", (unsigned)remote_addr->sa_family);
        _clear();
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }

    if (_fd < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // try binding
    int status = bind(_fd, (struct sockaddr *)&sock_addr, _remote_addr_len);
    if (status < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // get local port
    if (AF_INET == _remote_addr.ss_family)
    {
        DEBUG("read sock info by getsockname()");
        struct sockaddr_in addr = {0};
        socklen_t socklen = sizeof(addr);
        getsockname(_fd, (struct sockaddr *)(&addr), &socklen);
        ((struct sockaddr_in *)&_self_addr)->sin_port = addr.sin_port;
        _port = ntohs(addr.sin_port);
    }
    else // if (AF_INET6 == _remote_addr.ss_family)
    {
        DEBUG("read sock info by getsockname()");
        struct sockaddr_in6 addr = {0};
        socklen_t socklen = sizeof(addr);
        getsockname(_fd, (struct sockaddr *)(&addr), &socklen);
        ((struct sockaddr_in6 *)&_self_addr)->sin6_port = addr.sin6_port;
        _port = ntohs(addr.sin6_port);
    }

    // non-block
    set_fd_nonblock(_fd);
    arg->fd = _fd;

    // create event
    _owner_base = base;
    _event = event_new(base->event_base(), _fd, EV_TIMEOUT | EV_READ, _libevent_callback, arg);     // should NOT use EV_ET or EV_PERSIST
    if (NULL == _event) {
        ERROR("Failed to new a UDP session");
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }
    else {
        struct timeval sleep_time = {0, 0};
        DEBUG("Add UDP session %s", _identifier.c_str());
        event_add(_event, &sleep_time);
    }

    // automatic free
    if (auto_free) {
        base->put_event_under_control(this);
    }

    _status.clear_err();
    return _status;
}


#endif  // end of __PUBLIC_INIT_AND_CLEAR_FUNCTIONS


// ==========
#define __MISC_FUNCTIONS
#ifdef __MISC_FUNCTIONS

NetType_t UDPItnlSession::network_type()
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


int UDPItnlSession::port() const
{
    return _port;
}


struct stCoRoutine_t *UDPItnlSession::_coroutine()
{
    if (_event_arg) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        return arg->coroutine;
    }
    else {
        return NULL;
    }
}


#endif  // end of __MISC_FUNCTIONS


// ==========
#define __SLEEP_FUNCTIONS
#ifdef __SLEEP_FUNCTIONS


struct Error UDPItnlSession::sleep(struct timeval &sleep_time)
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


struct Error UDPItnlSession::sleep_milisecs(unsigned mili_secs)
{
    struct timeval timeout = to_timeval_from_milisecs(mili_secs);
    return sleep(timeout);
}


struct Error UDPItnlSession::sleep(double seconds)
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
#define __RECV_FUNCTIONS
#ifdef __RECV_FUNCTIONS

struct Error UDPItnlSession::recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out, const struct timeval &timeout)
{
    ssize_t recv_len = 0;
    uint32_t libevent_what = (_libevent_what_storage) ? *_libevent_what_storage : 0;
    struct _EventArg *arg = (struct _EventArg *)_event_arg;

    if (NULL == data_out) {
        ERROR("no recv data buffer spectied");
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    _status.clear_err();

    // ----------
    // recvfrom and get data
    // is there any data to be read?
    if (_data_len_to_read > 0)
    {
        if (len_limit < _data_len_to_read) {
            recv_len = len_limit;
        }
        else {
            recv_len = _data_len_to_read;
        }
        memcpy(data_out, (uint8_t *)_data_buff.c_data() + _data_offset, recv_len);
        _data_offset += recv_len;
        _data_len_to_read -= recv_len;

        // if all data read?
        if (_data_offset >= _data_buff.length()) {
            _data_buff.clear();
            _data_offset = 0;
            _data_len_to_read = 0;
        }
    }
    // recv from socket
    else if (event_readable(libevent_what))
    {
        // data incoming
        struct sockaddr_storage addr;
        socklen_t sock_len = _remote_addr_len;
        size_t data_len = 0;
        ssize_t recv_stat = recv_from(_fd, (void *)&data_len, sizeof(data_len), 0, (struct sockaddr *)&addr, &sock_len);
        if (recv_stat < 0) {
            _status.set_sys_errno();
        }
        else if (0 == recv_stat) {
            DEBUG("EAGAIN");
            *_libevent_what_storage &= ~EV_READ;
            return recv_in_timeval(data_out, len_limit, len_out, timeout);
        }
        else {
            // read data length and then fetch data
            if (len_limit < _data_len_to_read) {
                _data_len_to_read = data_len - len_limit;
                recv_len = len_limit;
            }
            else {
                _data_len_to_read = 0;
                recv_len = data_len;
            }
            memcpy(data_out, (uint8_t *)_data_buff.c_data() + _data_offset, recv_len);
            _data_offset += recv_len;

            // if all data read?
            if (_data_offset >= _data_buff.length()) {
                _data_buff.clear();
                _data_offset = 0;
                _data_len_to_read = 0;
            }
        }
    }
    // no data available, now wait
    else {
        // no data avaliable
        struct timeval timeout_copy;
        timeout_copy.tv_sec = timeout.tv_sec;
        timeout_copy.tv_usec = timeout.tv_usec;

        DEBUG("UDP libevent what flag: 0x%04x, now wait", (unsigned)libevent_what);
        if ((0 == timeout_copy.tv_sec) && (0 == timeout_copy.tv_usec)) {
            timeout_copy.tv_sec = FOREVER_SECONDS;
        }
        event_add(_event, &timeout_copy);
        co_yield(arg->coroutine);

        // check if data read
        libevent_what = (_libevent_what_storage) ? *_libevent_what_storage : 0;
        if (event_readable(libevent_what))
        {
            // data incoming
            struct sockaddr_storage addr;
            socklen_t sock_len = _remote_addr_len;
            size_t data_len = 0;
            ssize_t recv_stat = recv_from(_fd, (void *)&data_len, sizeof(data_len), 0, (struct sockaddr *)&addr, &sock_len);
            if (recv_stat < 0) {
                _status.set_sys_errno();
            }
            else {
                // read data length and then fetch data
                if (len_limit < _data_len_to_read) {
                    _data_len_to_read = data_len - len_limit;
                    recv_len = len_limit;
                }
                else {
                    _data_len_to_read = 0;
                    recv_len = data_len;
                }
                memcpy(data_out, (uint8_t *)_data_buff.c_data() + _data_offset, recv_len);
                _data_offset += recv_len;

                // if all data read?
                if (_data_offset >= _data_buff.length()) {
                    _data_buff.clear();
                    _data_offset = 0;
                    _data_len_to_read = 0;
                }
            }
        }
        else if (event_is_timeout(libevent_what))
        {
            recv_len = 0;
            _status.set_app_errno(ERR_TIMEOUT);
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


struct Error UDPItnlSession::recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out, unsigned mili_secs)
{
    struct timeval timeout = to_timeval_from_milisecs(mili_secs);
    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}


struct Error UDPItnlSession::recv(void *data_out, const size_t len_limit, size_t *len_out, double timeout_seconds)
{
    struct timeval timeout = {0, 0};
    if (timeout_seconds > 0) {
        timeout = to_timeval(timeout_seconds);
    }

    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}


struct Error UDPItnlSession::forward_incoming_data(const void *c_data, size_t data_len)
{
    if (!(c_data && data_len)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    _status.clear_err();
    ssize_t send_ret = sendto(_fd, (void *)&data_len, sizeof(data_len), 0, (struct sockaddr *)&_self_addr, _remote_addr_len);
    if (send_ret < 0) {
        _status.set_sys_errno();
        return _status;
    }

    _data_buff.append(c_data, data_len);
    return _status;
}

#endif  // end of __RECV_FUNCTIONS


// ==========
#define __SEND_FUNCTIONS
#ifdef __SEND_FUNCTIONS

struct Error UDPItnlSession::reply(const void *data, const size_t data_len, size_t *send_len_out)
{
    if (!(data && data_len)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }
    ssize_t send_stat = sendto(_server_fd, data, data_len, 0, (struct sockaddr *)&_remote_addr, _remote_addr_len);
    if (send_stat < 0) {
        _status.set_sys_errno();
    }
    else {
        _status.clear_err();
    }

    if (send_len_out) {
        *send_len_out = send_stat > 0 ? send_stat : 0;
    }
    return _status;
}


#endif


// ==========
#define __ADDR_FUNCTIONS
#ifdef __ADDR_FUNCTIONS

std::string UDPItnlSession::remote_addr()
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


unsigned UDPItnlSession::remote_port()
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


void UDPItnlSession::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    if (addr_out && addr_len <= sizeof(_remote_addr)) {
        memcpy((void *)addr_out, &_remote_addr, addr_len);
    }
    return;
}


#endif  // end of __ADDR_FUNCTIONS

