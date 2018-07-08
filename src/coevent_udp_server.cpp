
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

typedef Server _super;

// ==========
// necessary definitions
#define __CO_EVENT_UDP_DEFINITIONS
#ifdef __CO_EVENT_UDP_DEFINITIONS

struct _EventArg {
    UDPServer            *event;
    int                 fd;
    uint32_t            *libevent_what_ptr;

    struct stCoRoutine_t *coroutine;
    WorkerFunc          worker_func;
    void                *user_arg;

    WorkerFunc          session_worker_func;
    void                *session_user_arg;

    std::map<std::string, UDPItnlSession *> session_collection;

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
        UDPServer *event = arg->event;
        Base *base  = event->owner();

        DEBUG("evudp %s ends", event->identifier().c_str());
        base->delete_event_under_control(event);
    }

    // done
    return;
}

#endif


// ==========
#define __SESSION_MODE_WORKER_FUNC
#ifdef __SESSION_MODE_WORKER_FUNC

static void _session_mode_worker(evutil_socket_t fd, Event *abs_server, void *libcoevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libcoevent_arg;
    void *user_arg = arg->session_user_arg;
    WorkerFunc worker_func = arg->session_worker_func;
    UDPServer *server = (UDPServer *)abs_server;
    std::map<std::string, UDPItnlSession *> &session_collection = arg->session_collection;

    uint8_t data_buff[4096];
    size_t recv_len = 0;
    Error status;
    BOOL should_exit_udp = FALSE;

    socklen_t sock_len = 0;
    switch(server->network_type())
    {
        default:
        case NetIPv4:
            sock_len = sizeof(struct sockaddr_in);
            break;
        case NetIPv6:
            sock_len = sizeof(struct sockaddr_in6);
            break;
    }

    // receive data
    do {
        recv_len = 0;
        status = server->recv((void *)data_buff, sizeof(data_buff), &recv_len);
        if (status.is_ok() && (recv_len > 0))
        {
            std::string remote_ip = server->client_addr();
            unsigned remote_port = server->client_port();

            DEBUG("Got data: %s", ::andrewmc::cpptools::dump_data_to_string(data_buff, recv_len).c_str());

            char remote_key_buff[128];
            snprintf(remote_key_buff, sizeof(remote_key_buff), "%s:%u", remote_ip.c_str(), remote_port);

            std::string remote_key = std::string(remote_key_buff);

            std::map<std::string, UDPItnlSession *>::iterator session_iter = session_collection.find(remote_key);
            if (session_collection.end() != session_iter) {
                session_iter->second->forward_incoming_data(data_buff, recv_len);
            }
            else {
                // we should create a session
                struct sockaddr_storage sock_addr;
                server->copy_client_addr((struct sockaddr *)&sock_addr, sock_len);

                UDPItnlSession *session = new UDPItnlSession;
                status = session->init(server->owner(), fd, worker_func, (struct sockaddr *)&sock_addr, sock_len, user_arg);
                if (FALSE == status.is_ok()) {
                    delete session;
                    ERROR("Failed to create UDP session: %s", status.c_err_msg());
                }
                else {
                    DEBUG("Create session for %s", remote_key.c_str());
                    session->forward_incoming_data(data_buff, recv_len);
                    session_collection[remote_key] = session;
                }
            }
        }

    } while (FALSE == should_exit_udp);

    // exit UDP server
    return;
}


#endif


// ==========
#define __INTERNAL_MISC_FUNCTIONS
#ifdef __INTERNAL_MISC_FUNCTIONS

uint32_t UDPServer::_libevent_what()
{
    uint32_t ret = *_libevent_what_storage;
    *_libevent_what_storage = 0;
    return ret;
}


int UDPServer::_fd()
{
    if (_fd_ipv4) {
        return _fd_ipv4;
    } else if (_fd_ipv6) {
        return _fd_ipv6;
    } else if (_fd_unix) {
        return _fd_unix;
    } else {
        ERROR("file descriptor not initialized");
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return -1;
    }
}


struct sockaddr *UDPServer::_remote_sock_addr()
{
    if (_fd_ipv4) {
        return (struct sockaddr *)(&_remote_addr_ipv4);
    } else if (_fd_ipv6) {
        return (struct sockaddr *)(&_remote_addr_ipv6);
    } else if (_fd_unix) {
        return (struct sockaddr *)(&_remote_addr_unix);
    } else {
        ERROR("file descriptor not initialized");
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return (struct sockaddr *)(&_remote_addr_unix);
    }
}


socklen_t *UDPServer::_remote_sock_addr_len()
{
    if (_fd_ipv4) {
        return &_remote_addr_ipv4_len;
    } else if (_fd_ipv6) {
        return &_remote_addr_ipv6_len;
    } else if (_fd_unix) {
        return &_remote_addr_unix_len;
    } else {
        ERROR("file descriptor not initialized");
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return &_remote_addr_unix_len;
    }
}


#endif  // end of __INTERNAL_MISC_FUNCTIONS


// ==========
#define __PUBLIC_INIT_AND_CLEAR_FUNCTIONS
#ifdef __PUBLIC_INIT_AND_CLEAR_FUNCTIONS

struct Error UDPServer::init(Base *base, WorkerFunc func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg, BOOL auto_free)
{
    if (!(base && addr)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    if (addr->sa_family != AF_INET
        && addr->sa_family != AF_INET6
        && addr->sa_family != AF_UNIX)
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
    arg->libevent_what_ptr = _libevent_what_storage;
    DEBUG("arg->libevent_what_ptr = %p", arg->libevent_what_ptr);
    DEBUG("User arg: %08p", user_arg);

    // create arg for libco
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
    int status = bind(fd, addr, addr_len);
    if (status < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // non-block
    set_fd_nonblock(fd);
    arg->fd = fd;

    // create event
    _owner_base = base;
    _event = event_new(base->event_base(), fd, EV_TIMEOUT | EV_READ, _libevent_callback, arg);     // should NOT use EV_ET or EV_PERSIST
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

    // automatic free
    if (auto_free) {
        base->put_event_under_control(this);
    }

    _status.clear_err();
    return _status;
}


struct Error UDPServer::init(Base *base, WorkerFunc func, NetType_t network_type, int bind_port, void *user_arg, BOOL auto_free)
{
    if (NetIPv4 == network_type)
    {
        DEBUG("Init a IPv4 UDP event");
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons((unsigned short)bind_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        return init(base, func, (const struct sockaddr *)(&addr), sizeof(addr), user_arg, auto_free);
    }
    else if (NetIPv6 == network_type)
    {
        DEBUG("Init a IPv6 UDP event");
        struct sockaddr_in6 addr6;
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons((unsigned short)bind_port);
        addr6.sin6_addr = in6addr_any;
        return init(base, func, (const struct sockaddr *)(&addr6), sizeof(addr6), user_arg, auto_free);
    }
    else {
        ERROR("Invalid network type %d", (int)network_type);
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }
}


struct Error UDPServer::init(Base *base, WorkerFunc func, const char *bind_path, void *user_arg, BOOL auto_free)
{
    struct sockaddr_un addr;
    size_t path_len = 0;
    
    if (NULL == bind_path) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    path_len = strlen(bind_path);
    if (path_len + 1 > sizeof(addr.sun_path)) {
        _status.set_app_errno(ERR_BIND_PATH_ILLEGAL);
        return _status;
    }

    DEBUG("Init a local UDP event");
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, bind_path, path_len + 1);
    return init(base, func, (const struct sockaddr *)(&addr), sizeof(addr), user_arg, auto_free);
}


struct Error UDPServer::init(Base *base, WorkerFunc func, std::string &bind_path, void *user_arg, BOOL auto_free)
{
    return init(base, func, bind_path.c_str(), user_arg, auto_free);
}


void UDPServer::_init()
{
    char identifier[64];
    sprintf(identifier, "UDP server %p", this);
    _identifier = identifier;

    _fd_ipv4 = 0;
    _fd_ipv6 = 0;
    _fd_unix = 0;
    _remote_addr_ipv4_len = sizeof(_remote_addr_ipv4);
    _remote_addr_ipv6_len = sizeof(_remote_addr_ipv6);
    _remote_addr_unix_len = sizeof(_remote_addr_unix);
    _libevent_what_storage = NULL;
    _event_arg = NULL;

    if (NULL == _libevent_what_storage) {
        _libevent_what_storage = (uint32_t *)malloc(sizeof(*_libevent_what_storage));
        if (NULL == _libevent_what_storage) {
            ERROR("Failed to init storage");
            throw std::bad_alloc();
        }
        else {
            *_libevent_what_storage = 0;
            DEBUG("Init _libevent_what_storage OK: %p", _libevent_what_storage);
        }
    }
    return;
}


void UDPServer::_clear()
{
    if (_event) {
        DEBUG("Delete io event");
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

        for (std::map<std::string, UDPItnlSession *>::iterator each_session = (arg->session_collection).begin();
            each_session != arg->session_collection.end();
            each_session ++)
        {
            DEBUG("delete session %s", each_session->second->identifier().c_str());
            delete each_session->second;
        }
        arg->session_collection.clear();

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


UDPServer::UDPServer()
{
    _init();
    return;
}


UDPServer::~UDPServer()
{
    DEBUG("Delete UDP server %s", _identifier.c_str());
    _clear();

    if (_libevent_what_storage) {
        free(_libevent_what_storage);
        _libevent_what_storage = NULL;
    }

    return;
}

#endif  // end of __PUBLIC_INIT_AND_CLEAR_FUNCTIONS


// ==========
#define __INIT_SESSION_MODE
#ifdef __INIT_SESSION_MODE

struct Error UDPServer::init_session_mode(Base *base, WorkerFunc session_func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg, BOOL auto_free)
{
    if (!(base && addr && addr_len && session_func)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    if (addr->sa_family != AF_INET
        && addr->sa_family != AF_INET6)
    {
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }

    init(base, _session_mode_worker, addr, addr_len, user_arg, auto_free);
    if (_status.is_ok()) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        arg->user_arg = arg;        // expose struct _EventArg to _session_mode_worker()
        arg->session_worker_func = session_func;
        arg->session_user_arg = user_arg;
    }

    return _status;
}


struct Error UDPServer::init_session_mode(Base *base, WorkerFunc session_func, NetType_t network_type, int bind_port, void *user_arg, BOOL auto_free)
{
    if (!(base && bind_port && session_func)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    if (NetLocal == network_type) {
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }

    init(base, _session_mode_worker, network_type, bind_port, user_arg, auto_free);
    if (_status.is_ok()) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        arg->user_arg = arg;
        arg->session_worker_func = session_func;
        arg->session_user_arg = user_arg;
    }

    return _status;
}


#endif  // end of __INIT_SESSION_MODE


// ==========
#define __PUBLIC_MISC_FUNCTIONS
#ifdef __PUBLIC_MISC_FUNCTIONS

NetType_t UDPServer::network_type()
{
    if (_fd_ipv4) {
        return NetIPv4;
    } else if (_fd_ipv6) {
        return NetIPv6;
    } else if (_fd_unix) {
        return NetLocal;
    } else {
        return NetUnknown;
    }
}


const char *UDPServer::c_socket_path()
{
    if (_fd_unix) {
        return _sockaddr_unix.sun_path;
    } else {
        return "";
    }
}


int UDPServer::port()
{
    if (_fd_ipv4)
    {
        unsigned short port = ntohs(_sockaddr_ipv4.sin_port);
        if (0 == port) {
            DEBUG("read sock info by getsockname()");
            struct sockaddr_in addr = {0};
            socklen_t socklen = sizeof(addr);
            getsockname(_fd_ipv4, (struct sockaddr *)(&addr), &socklen);
            port = ntohs(addr.sin_port);
        }
        return (int)port;
    }
    if (_fd_ipv6)
    {
        unsigned short port = ntohs(_sockaddr_ipv6.sin6_port);
        if (0 == port) {
            DEBUG("read sock info by getsockname()");
            struct sockaddr_in6 addr = {0};
            socklen_t socklen = sizeof(addr);
            getsockname(_fd_ipv6, (struct sockaddr *)(&addr), &socklen);
            port = ntohs(addr.sin6_port);
        }
        return (int)port;
    }
    // else
    return -1;
}


struct stCoRoutine_t *UDPServer::_coroutine()
{
    if (_event_arg) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        return arg->coroutine;
    }
    else {
        return NULL;
    }
}


#endif  // end of __PUBLIC_MISC_FUNCTIONS


// ==========
#define __PUBLIC_FUNCTIONAL_FUNCTIONS
#ifdef __PUBLIC_FUNCTIONAL_FUNCTIONS


struct Error UDPServer::sleep(struct timeval &sleep_time)
{
    struct _EventArg *arg = (struct _EventArg *)_event_arg;

    _status.clear_err();
    if ((0 == sleep_time.tv_sec) && (0 == sleep_time.tv_usec)) {
        return _status;
    }

    event_add(_event, &sleep_time);
    co_yield(arg->coroutine);

    // determine libevent event masks
    uint32_t libevent_what = _libevent_what();
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


struct Error UDPServer::sleep_milisecs(unsigned mili_secs)
{
    struct timeval timeout = to_timeval_from_milisecs(mili_secs);
    return sleep(timeout);
}


struct Error UDPServer::sleep(double seconds)
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


struct Error UDPServer::recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out, const struct timeval &timeout)
{
    ssize_t recv_len = 0;
    uint32_t libevent_what = 0;
    struct _EventArg *arg = (struct _EventArg *)_event_arg;

    // param check
    if (NULL == data_out) {
        ERROR("no recv data buffer spectied");
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }
    _status.clear_err();

    // recvfrom()
    libevent_what = _libevent_what();
    if (event_readable(libevent_what))
    {
        // data readable
        recv_len = recv_from(_fd(), data_out, len_limit, 0, _remote_sock_addr(), _remote_sock_addr_len());
        if (recv_len < 0) {
            _status.set_sys_errno();
        }

        // EAGAIN
        if (0 == recv_len) {
            DEBUG("EAGAIN");
            *_libevent_what_storage &= ~EV_READ;
            return recv_in_timeval(data_out, len_limit, len_out, timeout);
        }
    }
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
        libevent_what = _libevent_what();
        if (event_readable(libevent_what))
        {
            recv_len = recv_from(_fd(), data_out, len_limit, 0, _remote_sock_addr(), _remote_sock_addr_len());
            if (recv_len < 0) {
                _status.set_sys_errno();
            }
            else if (0 == recv_len) {
                *_libevent_what_storage &=~ (EV_READ);
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


struct Error UDPServer::recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out, unsigned mili_secs)
{
    struct timeval timeout = to_timeval_from_milisecs(mili_secs);
    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}


struct Error UDPServer::recv(void *data_out, const size_t len_limit, size_t *len_out, double timeout_seconds)
{
    struct timeval timeout = {0, 0};
    if (timeout_seconds > 0) {
        timeout = to_timeval(timeout_seconds);
    }

    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}


std::string UDPServer::client_addr()
{
    if (_fd_ipv4)
    {
        char c_addr_str[INET_ADDRSTRLEN + 1];
        c_addr_str[INET_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET, &(_remote_addr_ipv4.sin_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else if (_fd_ipv6)
    {
        char c_addr_str[INET6_ADDRSTRLEN + 1];
        c_addr_str[INET6_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET6, &(_remote_addr_ipv6.sin6_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else {
        return "";
    }
}


unsigned UDPServer::client_port()
{
    if (_fd_ipv4) {
        return (unsigned)ntohs(_remote_addr_ipv4.sin_port);
    }
    else if (_fd_ipv6) {
        return (unsigned)ntohs(_remote_addr_ipv6.sin6_port);
    }
    else {
        return 0;
    }
}


void UDPServer::copy_client_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    if (NULL == addr_out) {
        return;
    }
    else {
        socklen_t remote_addr_len = *_remote_sock_addr_len();
        memcpy(addr_out, _remote_sock_addr(), remote_addr_len <= addr_len ? remote_addr_len : addr_len);
        return;
    }
}


struct Error UDPServer::send(const void *data, const size_t data_len, size_t *send_len_out, const struct sockaddr *addr)
{
    _status.clear_err();
    if (NULL == data || 0 == data_len) {
        ERROR("no data to send");
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    // prepare parameters
    ssize_t send_ret = 0;
    socklen_t addr_len;
    int fd = _fd();

    if (NULL == addr) {
        addr = _remote_sock_addr();
    }

    if (_fd_ipv4) {
        addr_len = _remote_addr_ipv4_len;
    }
    else if (_fd_ipv6) {
        addr_len = _remote_addr_ipv6_len;
    }
    else if (_fd_unix) {
        addr_len = _remote_addr_unix_len;
    }

    // sendto()
    if (fd > 0) 
    {
        send_ret = sendto(fd, data, data_len, 0, addr, addr_len);
        if (send_ret < 0) {
            _status.set_sys_errno();
        }
    }

    // return
    if (send_len_out) {
        if (send_ret > 0) {
            *send_len_out = send_ret;
        } else {
            *send_len_out = 0;
        }
    }
    return _status;
}


struct Error UDPServer::send(const void *data, const size_t data_len, size_t *send_len_out, const std::string &target_address, unsigned port)
{
    if (0 == target_address.length()) {
        return send(data, data_len, send_len_out, NULL);        // reply
    }
    else {
        if (_fd_ipv4) {
            struct sockaddr_in addr;
            convert_str_to_sockaddr_in(target_address, port, &addr);
            return send(data, data_len, send_len_out,(struct sockaddr *)(&addr));
        }
        else if (_fd_ipv4) {
            struct sockaddr_in6 addr;
            convert_str_to_sockaddr_in6(target_address, port, &addr);
            return send(data, data_len, send_len_out, (struct sockaddr *)(&addr));
        }
        else if (_fd_unix) {
            return send(data, data_len, send_len_out, NULL);    // reply
        }
        else {
            _status.set_app_errno(ERR_NOT_INITIALIZED);
            return _status;
        }
    }
}


struct Error UDPServer::reply(const void *data, const size_t data_len, size_t *reply_len_out)
{
    return send(data, data_len, reply_len_out, NULL);
}

#endif
