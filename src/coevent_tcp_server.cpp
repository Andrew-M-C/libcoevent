
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
#define __CO_EVENT_TCP_LIBEVENT_ARGS
#ifdef __CO_EVENT_TCP_LIBEVENT_ARGS

struct _EventArg {
    TCPServer           *server;
    WorkerFunc          session_worker_func;
    void                *session_user_arg;
    socklen_t           sock_len;
    std::map<int, TCPSession *> *sessions;

    // TCP server supports session mode ONLY, therefore no coroutine needed.
};

#endif  // end of __CO_EVENT_TCP_LIBEVENT_ARGS


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __CO_EVENT_UDP_CALLBACK
#ifdef __CO_EVENT_UDP_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;
    TCPServer *server = arg->server;
    BOOL should_end_server = FALSE;
    uint32_t libevent_what = (uint32_t)what;

    DEBUG("TCP server libevent what: 0x%04x - %s%s%s", (unsigned)what, event_is_timeout(what) ? "timeout " : "", event_readable(what) ? "read" : "", event_got_signal(what) ? "signal" : "");

    // should exit?
    if (event_got_signal(libevent_what))
    {
        should_end_server = TRUE;
    }
    else if (event_readable(libevent_what))
    {
        struct sockaddr_storage remote_addr;
        socklen_t sock_len = arg->sock_len;
        int client_fd = accept(fd, (struct sockaddr *)&remote_addr, &sock_len);

        TCPItnlSession *session = new TCPItnlSession;
        if (NULL == session) {
            ERROR("Failed to create a TCP session");
            close(client_fd);
        }
        else {
            Error status = session->init(server, client_fd, arg->session_worker_func, (struct sockaddr *)&remote_addr, sock_len, arg->session_user_arg);
            if (FALSE == status.is_ok()) {
                ERROR("Failed to init TCP session: %s", status.c_err_msg());
                close(client_fd);
                delete session;
                session = NULL;
            }
            else {
                (*(arg->sessions))[client_fd] = (TCPSession *)session;
            }
        }
    }
    else {
        ERROR("Unrecognized event flag: 0x%02x", (unsigned)libevent_what);
        should_end_server = TRUE;
    }

    // is coroutine end?
    if (should_end_server)
    {
        DEBUG("evudp %s ends", server->identifier().c_str());
        server->owner()->delete_event_under_control(server);
    }

    // done
    return;
}

#endif


// ==========
#define __CONSTRUCTOR_AND_DESTRUCTOR
#ifdef __CONSTRUCTOR_AND_DESTRUCTOR

void TCPServer::_clear()
{
    if (_event) {
        DEBUG("Delete TCP io event");
        event_del(_event);
        _event = NULL;
    }

    if (_event_arg) {
        free(_event_arg);
        _event_arg = NULL;
    }

    for (std::map<int, TCPSession *>::iterator each_session = _sessions.begin();
        each_session != _sessions.end();
        each_session ++)
    {
        DEBUG("Delete TCP session: %s", each_session->second->identifier().c_str());
        delete each_session->second;
    }
    _sessions.clear();

    if (_fd > 0) {
        close(_fd);
        _fd = 0;
    }

    _sock_addr.ss_family = (sa_family_t)0;
    _sock_addr_len = 0;
    _port = 0;
    return;
}


TCPServer::TCPServer()
{
    _event_arg = NULL;
    _fd = 0;
    _sock_addr_len = 0;
    _port = 0;
    return;
}


TCPServer::~TCPServer()
{
    _clear();
    return;
}


#endif  // end of __CONSTRUCTOR_AND_DESTRUCTOR


// ==========
#define __INIT_FUNCTIONS
#ifdef __INIT_FUNCTIONS

struct Error TCPServer::init_session_mode(Base *base, WorkerFunc session_func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg, BOOL auto_free)
{
    if (!(base && session_func && addr && addr_len)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    switch (addr->sa_family)
    {
        case AF_INET:
            _sock_addr_len = sizeof(struct sockaddr_in);
            break;
        case AF_INET6:
            _sock_addr_len = sizeof(struct sockaddr_in6);
            break;
        case AF_UNIX:
            _sock_addr_len = sizeof(struct sockaddr_un);
            break;
        default:
            _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
            return _status;
            break;
    }

    if (addr_len < _sock_addr_len) {
        _clear();
        _status.set_app_errno(ERR_PARA_ILLEGAL);
        return _status;
    }

    _clear();

    // create arguments
    struct _EventArg *arg = (struct _EventArg *)malloc(sizeof(struct _EventArg));
    if (NULL == arg) {
        throw std::bad_alloc();
        _status.set_sys_errno();
        return _status;
    }

    _event_arg = arg;
    arg->server = this;
    arg->session_worker_func = session_func;
    arg->session_user_arg = user_arg;
    arg->sock_len = _sock_addr_len;
    arg->sessions = &_sessions;

    // init sockets
    memcpy(&_sock_addr, addr, _sock_addr_len);
    _fd = socket(addr->sa_family, SOCK_DGRAM, 0);
    if (_fd < 0) {
        _clear();
        _status.set_sys_errno();
        return _status;
    }

    // try binding
    int status = bind(_fd, (struct sockaddr *)&_sock_addr, _sock_addr_len);
    if (status < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // nonblock
    set_fd_nonblock(_fd);

    // read port
    if (AF_INET == addr->sa_family)
    {
        struct sockaddr_in addr_buff;
        socklen_t sock_len = sizeof(addr_buff);
        getsockname(_fd, (struct sockaddr *)(&addr_buff), &sock_len);
        _port = (unsigned)ntohs(addr_buff.sin_port);
    }
    else if (AF_INET6 == addr->sa_family)
    {
        struct sockaddr_in6 addr6_buff;
        socklen_t sock_len = sizeof(addr6_buff);
        getsockname(_fd, (struct sockaddr *)(&addr6_buff), &sock_len);
        _port = (unsigned)ntohs(addr6_buff.sin6_port);
    }

    // create event
    _owner_base = base;
    _event = event_new(base->event_base(), _fd, EV_READ, _libevent_callback, arg);
    if (NULL == _event) {
        ERROR("Failed to new a TCP server event");
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }
    event_add(_event, NULL);

    // automatic free
    if (auto_free) {
        base->put_event_under_control(this);
    }

    // end
    _status.clear_err();
    return _status;
}


struct Error TCPServer::init_session_mode(Base *base, WorkerFunc session_func, NetType_t network_type, int bind_port, void *user_arg, BOOL auto_free)
{
    if (NetIPv4 == network_type)
    {
        DEBUG("Init a IPv4 TCP server");
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons((unsigned short)bind_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        return init_session_mode(base, session_func, (const struct sockaddr *)(&addr), sizeof(addr), user_arg, auto_free);
    }
    else if (NetIPv6 == network_type)
    {
        DEBUG("Init a IPv6 TCP server");
        struct sockaddr_in6 addr6;
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons((unsigned short)bind_port);
        addr6.sin6_addr = in6addr_any;
        return init_session_mode(base, session_func, (const struct sockaddr *)(&addr6), sizeof(addr6), user_arg, auto_free);
    }
    else {
        ERROR("Invalid network type %d", (int)network_type);
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }
}


struct Error TCPServer::init_session_mode(Base *base, WorkerFunc session_func, const char *bind_path, void *user_arg, BOOL auto_free)
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

    DEBUG("Init a local TCP server");
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, bind_path, path_len + 1);
    return init_session_mode(base, session_func, (const struct sockaddr *)(&addr), sizeof(addr), user_arg, auto_free);
}


struct Error TCPServer::init_session_mode(Base *base, WorkerFunc session_func, std::string &bind_path, void *user_arg, BOOL auto_free)
{
    return init_session_mode(base, session_func, bind_path.c_str(), user_arg, auto_free);
}


#endif  // end of __INIT_FUNCTIONS


// ==========
#define __TCP_QUIT_FUNCTIONS
#ifdef __TCP_QUIT_FUNCTIONS

struct Error TCPServer::quit_session_mode_server()
{
    if (NULL == _event_arg) {
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return _status;
    }
    _status.clear_err();

    event_active(_event, EV_SIGNAL, 0);
    return _status;
}


struct Error TCPServer::notify_session_ends(TCPSession *the_session)
{
    TCPItnlSession *session = (TCPItnlSession *)the_session;
    std::map<int, TCPSession *>::iterator session_in_control = _sessions.find(session->file_descriptor());
    
    if (_sessions.end() != session_in_control)
    {
        DEBUG("dispatch session %s", session_in_control->second->identifier().c_str());
        _sessions.erase(session_in_control);
        _status.clear_err();
    }
    else {
        _status.set_app_errno(ERR_OBJ_NOT_FOUND);
    }

    return _status;
}


// ==========
#define __MISC_FUNCTIONS
#ifdef __MISC_FUNCTIONS

NetType_t TCPServer::network_type()
{
    switch(_sock_addr.ss_family)
    {
        case AF_INET:
            return NetIPv4;
            break;
        case AF_INET6:
            return NetIPv6;
            break;
        case AF_UNIX:
            return NetLocal;
            break;
        default:
            return NetUnknown;
            break;
    }
}


const char *TCPServer::c_socket_path()
{
    if (AF_UNIX == _sock_addr.ss_family) {
        struct sockaddr_un *addr = (struct sockaddr_un *)&_sock_addr;
        return addr->sun_path;
    }
    else {
        return "";
    }
}


int TCPServer::port()
{
    return (int)_port;
}


#endif


#endif


