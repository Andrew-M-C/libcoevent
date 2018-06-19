
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
#define __CO_EVENT_UDP_CLIENT_DEFINITIONS
#ifdef __CO_EVENT_UDP_CLIENT_DEFINITIONS

struct _EventArg {
    UDPItnlClient       *event;
    int                 fd;
    uint32_t            *libevent_what_ptr;

    struct stCoRoutine_t *coroutine;
    void                *user_arg;

    _EventArg(): event(NULL), fd(0), libevent_what_ptr(NULL), coroutine(NULL)
    {}
};

#endif


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __UDP_CLIENT_SERVER_CALLBACK
#ifdef __UDP_CLIENT_SERVER_CALLBACK

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
        UDPItnlClient *event = arg->event;
        Base *base = event->owner();

        DEBUG("evudp %s ends", event->identifier().c_str());
        base->delete_event_under_control(event);
    }

    // done
    return;
}

#endif

// ==========
#define __CONSTRUCT_AND_DESTRUCT
#ifdef __CONSTRUCT_AND_DESTRUCT


UDPItnlClient::UDPItnlClient()
{
    _init();
    return;
}


UDPItnlClient::~UDPItnlClient()
{
    _clear();

    if (_libevent_what_storage) {
        free(_libevent_what_storage);
        _libevent_what_storage = NULL;
    }
    return;
}


void UDPItnlClient::_init()
{
    char identifier[64];
    sprintf(identifier, "UDP client %p", this);
    _identifier = identifier;

    _owner_server = NULL;
    _fd_ipv4 = 0;
    _fd_ipv6 = 0;
    _fd_unix = 0;
    _remote_addr_ipv4_len = sizeof(_remote_addr_ipv4);
    _remote_addr_ipv6_len = sizeof(_remote_addr_ipv6);
    _remote_addr_unix_len = sizeof(_remote_addr_unix);

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


void UDPItnlClient::_clear()
{
    if (_event) {
        DEBUG("Delete UDP client");
        event_del(_event);
        _event = NULL;
    }

    if (_event_arg) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        _event_arg = NULL;

        // do not remove coroutine because client does not own it

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


#endif  // end of __INIT_FUNCTIONS


// ==========
#define __MISC_FUNCTIONS
#ifdef __MISC_FUNCTIONS

NetType_t UDPItnlClient::network_type()
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


std::string UDPItnlClient::remote_addr()
{
    // TODO:
    return "";
}


unsigned UDPItnlClient::remote_port()
{
    // TODO:
    return 0;
}


void UDPItnlClient::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    // TODO:
    return;
}

#endif  // end of __MISC_FUNCTIONS


// ==========
#define __INTERFACE_INIT
#ifdef __INTERFACE_INIT

struct Error UDPItnlClient::init(Server *server, struct stCoRoutine_t *coroutine, NetType_t network_type, void *user_arg)
{
    if (!(server && coroutine)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    switch(network_type)
    {
        case NetIPv4:
        case NetIPv6:
        case NetLocal:
            // OK
            break;

        case NetUnknown:
        default:
            // Illegal
            _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
            return _status;
            break;
    }

    _clear();
    _status.clear_err();

    // create arguments and assign coroutine
    int fd = 0;
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    struct sockaddr_un addrun;
    struct sockaddr *addr = NULL;
    socklen_t addr_len = 0;

    struct _EventArg *arg = new _EventArg;
    _event_arg = arg;
    arg->event = this;
    arg->user_arg = user_arg;
    arg->libevent_what_ptr = _libevent_what_storage;
    arg->coroutine = coroutine;

    // create file descriptor
    if (NetIPv4 == network_type)
    {
        addr4.sin_family = AF_INET;
        addr4.sin_port = 0;
        addr4.sin_addr.s_addr = htonl(INADDR_ANY);

        _fd_ipv4 = socket(AF_INET, SOCK_DGRAM, 0);
        fd = _fd_ipv4;
        addr = (struct sockaddr *)(&addr4);
        addr_len = sizeof(addr4);
    }
    else if (NetIPv6 == network_type)
    {
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = 0;
        addr6.sin6_addr = in6addr_any;

        _fd_ipv6 = socket(AF_INET6, SOCK_DGRAM, 0);
        fd = _fd_ipv6;
        addr = (struct sockaddr *)(&addr6);
        addr_len = sizeof(addr6);
    }
    else    // NetLocal, AF_UNIX
    {
        addrun.sun_family = AF_UNIX;
        _fd_unix = socket(AF_UNIX, SOCK_DGRAM, 0);
        fd = _fd_unix;
        addr = (struct sockaddr *)(&addrun);
        addr_len = sizeof(addrun);
    }

    if (fd <= 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // try binding
    int status = bind(fd, addr, addr_len);
    if (status < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // non-block
    set_fd_nonblock(fd);

    // create event
    _owner_server = server;
    _owner_base = server->owner();
    _event = event_new(_owner_base->event_base(), fd, EV_TIMEOUT | EV_READ, _libevent_callback, arg);
    if (NULL == _event) {
        ERROR("Failed to new a UDP event");
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }
    else {
        // Do NOT need to auto_free because auto_free is done in this class
        // base->put_event_under_control(this);
    }

    // done
    return _status;
}

#endif  // end of __INTERFACE_INIT


// ==========
#define __SEND_FUNCTION
#ifdef __SEND_FUNCTION

struct Error UDPItnlClient::send(const void *data, const size_t data_len, size_t *send_ken_out)
{
    // TODO:
    return _status;
}

#endif  // end of __SEND_FUNCTION


// ==========
#define __RECV_FUNCTION
#ifdef __RECV_FUNCTION

struct Error UDPItnlClient::recv(void *data_out, const size_t len_limie, size_t *len_out, double timeout_seconds)
{
    // TODO:
    return _status;
}


struct Error UDPItnlClient::recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out, const struct timeval &timeout)
{
    // TODO:
    return _status;
}


struct Error UDPItnlClient::recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out, unsigned timeout_milisecs)
{
    // TODO:
    return _status;
}

#endif


// end of file
