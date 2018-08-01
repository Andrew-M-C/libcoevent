// file encoding: UTF-8

#ifndef __CO_EVENT_H__
#define __CO_EVENT_H__

#include "coevent_const.h"
#include "event2/event.h"
#include "co_routine.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <set>
#include <map>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

namespace andrewmc {
namespace libcoevent {

class Base;
class Event;
class Server;
class Client;

class UDPServer;
class UDPClient;
class DNSClient;
class UDPSession;

class TCPServer;
class TCPSession;
class TCPClient;


// network type
typedef enum {
    NetUnknown = 0,
    NetIPv4,
    NetIPv6,
    NetLocal,
} NetType_t;


// coroutine function
typedef void (*WorkerFunc)(evutil_socket_t, Event *, void *);


// libcoevent use this structure to return error information
struct Error {
private:
    uint16_t _sys_errno;    // error defined in "errno.h"
    uint16_t _lib_errno;    // error defined by libcoevent in coevent_const.h
    const char *_err_msg;

public:
    Error():
        _sys_errno(0),_lib_errno(0),_err_msg(NULL)
    {}

    BOOL is_error();
    BOOL is_OK();
    BOOL is_ok();
    BOOL is_timeout();
    void clear_err();
    void set_sys_errno();
    void set_sys_errno(int sys_errno);
    void set_app_errno(ErrCode_t lib_errno);
    void set_app_errno(ErrCode_t lib_errno, const char *c_err_msg);
    void set_app_errno(ErrCode_t lib_errno, const std::string &err_msg);
    const char *c_err_msg();
    uint32_t err_code();
    uint32_t app_err_code(void);
    uint32_t sys_err_code(void);
};


// ====================
// event base of libcoevent
class Base {
    // private implementations
private:
    struct event_base   *_event_base;
    std::string         _identifier;
    std::set<Event *>   _events_under_control;  // User may put server event into a base, it will be deallocated automatically when this is not needed anymore.

    // constructor and destructors
public:
    Base();
    virtual ~Base();
    struct event_base *event_base();
    struct Error run();
    void set_identifier(std::string &identifier);
    const std::string &identifier();
    void put_event_under_control(Event *event);
    void delete_event_under_control(Event *event);
};


// ====================
// Base class of all libcoevent events
class Event {
protected:
    std::string     _identifier;
    Base            *_owner_base;
    struct event    *_event;
    struct Error    _status;

private:
    void            *_custom_storage;
    size_t          _custom_storage_size;

public:
    Event();
    virtual ~Event();

    void set_identifier(std::string &identifier);
    const std::string &identifier();

    struct Error create_custom_storage(size_t size);      // allocate customized storage to store user data. This storage will automatically freed when the object is destructed.
    void *custom_storage();
    size_t custom_storage_size();

    struct Error status();
    Base *owner();
};


// abstract event of servers
class Procedure : public Event {
protected:
    std::set<Client *>  _client_chain;
public:
    virtual ~Procedure();
    struct Error delete_client(Client *client);
    UDPClient *new_UDP_client(NetType_t network_type, void *user_arg = NULL);
    DNSClient *new_DNS_client(NetType_t network_type, void *user_arg = NULL);
    TCPClient *new_TCP_client(NetType_t network_type, void *user_arg = NULL);
protected:
    virtual struct stCoRoutine_t *_coroutine();
};


class Server : public Procedure {
public:
    Server(){};
    virtual ~Server(){};
};

class Session : public Procedure {
public:
    Session(){}
    virtual ~Session(){};
};


// ====================
// pure event, no network interfaces supported
class SubRoutine : public Server {
protected:
    void            *_event_arg;
public:
    SubRoutine();
    virtual ~SubRoutine();
    struct Error init(Base *base, WorkerFunc func, void *user_arg = NULL, BOOL auto_free = TRUE);

    struct Error sleep(double seconds);     // can ONLY be incoked inside coroutine
    struct Error sleep(const struct timeval &sleep_time);
    struct Error sleep_milisecs(unsigned mili_secs);
private:
    void _init();
    void _clear();
protected:
    struct stCoRoutine_t *_coroutine();
};


// abstract event of clients
// Client classes should ONLY been instantiated by Server objects 
// should NOT invoke delete to clients!!!
class Client : public Event {
public:
    Client(){};
    virtual ~Client(){};
    virtual std::string remote_addr() = 0;    // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;       // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;
};


// ====================
// UDP server
class UDPServer : public Server {
protected:
    void            *_event_arg;
    int             _fd_ipv4;
    int             _fd_ipv6;
    int             _fd_unix;
    struct sockaddr_in  _sockaddr_ipv4;
    struct sockaddr_in6 _sockaddr_ipv6;
    struct sockaddr_un  _sockaddr_unix;
    uint32_t        *_libevent_what_storage;    // ensure that this is assigned in heap instead of stack

    struct sockaddr_in  _remote_addr_ipv4;
    socklen_t           _remote_addr_ipv4_len;
    struct sockaddr_in6 _remote_addr_ipv6;
    socklen_t           _remote_addr_ipv6_len;
    struct sockaddr_un  _remote_addr_unix;
    socklen_t           _remote_addr_unix_len;

    std::map<std::string, UDPSession *> _session_collection;

public:
    UDPServer();
    virtual ~UDPServer();

    struct Error init(Base *base, WorkerFunc func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, NetType_t network_type, int bind_port = 0, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, const char *bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, std::string &bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);

    // session mode does not support AF_UNIX
    struct Error init_session_mode(Base *base, WorkerFunc session_func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, NetType_t network_type, int bind_port = 0, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error quit_session_mode_server();
    struct Error notify_session_ends(UDPSession *session);      // actually protected

    NetType_t network_type();
    const char *c_socket_path();    // valid in local type
    int port();                     // valid in IPv4 or IPv6 type

    struct Error sleep(double seconds);
    struct Error sleep(struct timeval &sleep_time);
    struct Error sleep_milisecs(unsigned mili_secs);

    struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable = NULL, double timeout_seconds = 0);
    struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout);
    struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs);

    struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const struct sockaddr *addr_nullable);
    struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL, const std::string &target_address = "", unsigned port = 80);
    struct Error reply(const void *data, const size_t data_len, size_t *reply_len_out = NULL);

    std::string client_addr();      // valid in IPv4 or IPv6 type
    unsigned client_port();         // valid in IPv4 or IPv6 type
    void copy_client_addr(struct sockaddr *addr_out, socklen_t addr_len);

private:
    void _init();
    void _clear();

    uint32_t _libevent_what();
    int _fd();
    struct sockaddr *_remote_sock_addr();
    socklen_t *_remote_sock_addr_len();
protected:
    struct stCoRoutine_t *_coroutine();
};


// UDP Session
class UDPSession : public Session {
public:
    UDPSession(){};
    virtual ~UDPSession(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error reply(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;
    virtual struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable, double timeout_seconds = 0) = 0;
    virtual struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout) = 0;
    virtual struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs) = 0;

    virtual struct Error sleep(double seconds) = 0;
    virtual struct Error sleep(struct timeval &sleep_time) = 0;
    virtual struct Error sleep_milisecs(unsigned mili_secs) = 0;

    virtual std::string remote_addr() = 0;      // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;         // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;
};


// UDP Client
class UDPClient : public Client {
public:
    UDPClient(){};
    virtual ~UDPClient(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const struct sockaddr *addr, socklen_t addr_len) = 0;
    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const std::string &target_address, unsigned target_port = 80) = 0;
    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const char *target_address = "", unsigned target_port = 80) = 0;
    virtual struct Error reply(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;

    virtual struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable, double timeout_seconds) = 0;
    virtual struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout) = 0;
    virtual struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs) = 0;

    virtual std::string remote_addr() = 0;    // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;       // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;

    virtual Procedure *owner_server() = 0;
};


// ====================
// DNSClient
class DNSResult;
class DNSResourceRecord;
class DNSClient : public Client {
public:
    DNSClient(){};
    virtual ~DNSClient(){};

    virtual NetType_t network_type() = 0;
    virtual struct Error resolve(const std::string &domain_name, double timeout_seconds = 0, const std::string &dns_server_ip = "") = 0;
    virtual struct Error resolve_in_timeval(const std::string &domain_name, const struct timeval &timeout, const std::string &dns_server_ip = "") = 0;
    virtual struct Error resolve_in_milisecs(const std::string &domain_name, unsigned timeout_milisecs, const std::string &dns_server_ip = "") = 0;

    virtual std::string default_dns_server(size_t index = 0, NetType_t *network_type_out = NULL) = 0;

    virtual const DNSResult *dns_result(const std::string &domain_name) = 0;
    virtual std::string quick_resolve(const std::string &domain_name, double timeout_seconds = 0, const std::string &dns_server_ip = "") = 0;
};


// DNSResult for DNSClient
typedef enum {
    DnsRRType_Unknown   = 0,
    DnsRRType_IPv4Addr  = 1,
    DnsRRType_IPv6Addr  = 28,
    DnsRRType_ServerName = 2,
    DnsRRType_CName     = 5,
} DNSRRType_t;

typedef enum {
    DnsRRClass_Unknown = 0,
    DnsRRClass_Internet = 1,
} DNSRRClass_t;


class DNSResult {
public:
    time_t      _update_time;   // saved as sysup time
    time_t      _update_ttl;    // saved as ttl remains comparing _update_time
    std::string _domain_name;
    std::vector<DNSResourceRecord *> _rr_list;
public:
    DNSResult();
    virtual ~DNSResult();
    const std::string &domain_name() const;
    std::vector<std::string> IP_addresses();
    time_t time_to_live() const;
    BOOL parse_from_udp_payload(const void *data, const size_t length);

    size_t resource_record_count() const;
    const DNSResourceRecord *resource_record(size_t index) const;
};


class DNSResourceRecord {
public:
    std::string     _rr_name;
    DNSRRType_t     _rr_type;
    DNSRRClass_t    _rr_class;
    uint32_t        _rr_ttl;
    std::string     _rr_address;
public:
    DNSResourceRecord():
        _rr_type(DnsRRType_Unknown), _rr_class(DnsRRClass_Unknown), _rr_ttl(0)
    {}

    const std::string &record_name() const;
    DNSRRType_t record_type() const;
    DNSRRClass_t record_class() const;
    uint32_t record_ttl() const;
    const std::string &record_address() const;
};


// ====================
// TCPServer
class TCPServer : public Server {
private:
    void                        *_event_arg;
    int                         _fd;
    struct sockaddr_storage     _sock_addr;
    socklen_t                   _sock_addr_len;
    std::map<int, TCPSession *> _sessions;
    unsigned                    _port;
public:
    TCPServer();
    virtual ~TCPServer();

    struct Error init_session_mode(Base *base, WorkerFunc session_func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, NetType_t network_type, int bind_port = 0, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, const char *bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, std::string &bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error quit_session_mode_server();
    struct Error notify_session_ends(TCPSession *session);      // actually protected

    NetType_t network_type();
    const char *c_socket_path();    // valid in local type
    int port();                     // valid in IPv4 or IPv6 type
private:
    void _clear();
};


// TCPSession
class TCPSession : public Session {
public:
    TCPSession(){};
    virtual ~TCPSession(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error reply(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;
    virtual struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable, double timeout_seconds = 0) = 0;
    virtual struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout) = 0;
    virtual struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs) = 0;

    virtual struct Error disconnect(void) = 0;

    virtual struct Error sleep(double seconds) = 0;
    virtual struct Error sleep(struct timeval &sleep_time) = 0;
    virtual struct Error sleep_milisecs(unsigned mili_secs) = 0;

    virtual std::string remote_addr() = 0;      // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;         // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;
};


// TCPClient
class TCPClient : public Client {
public:
    TCPClient(){};
    virtual ~TCPClient(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error connect_to_server(const struct sockaddr *addr, socklen_t addr_len, double timeout_seconds = 0) = 0;
    virtual struct Error connect_to_server(const std::string &target_address = "", unsigned target_port = 80, double timeout_seconds = 0) = 0;
    virtual struct Error connect_to_server(const char *target_address = "", unsigned target_port = 80, double timeout_seconds = 0) = 0;

    virtual struct Error connect_in_timeval(const struct sockaddr *addr, socklen_t addr_len, const struct timeval &timeout) = 0;
    virtual struct Error connect_in_timeval(const std::string &target_address, unsigned target_port, const struct timeval &timeout) = 0;
    virtual struct Error connect_in_timeval(const char *target_address, unsigned target_port, const struct timeval &timeout) = 0;

    virtual struct Error connect_in_mimlisecs(const struct sockaddr *addr, socklen_t addr_len, unsigned timeout_milisecs) = 0;
    virtual struct Error connect_in_mimlisecs(const std::string &target_address, unsigned target_port, unsigned timeout_milisecs) = 0;
    virtual struct Error connect_in_mimlisecs(const char *target_address, unsigned target_port, unsigned timeout_milisecs) = 0;

    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;

    virtual struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable, double timeout_seconds) = 0;
    virtual struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout) = 0;
    virtual struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs) = 0;

    virtual std::string remote_addr() = 0;    // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;       // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;

    virtual Procedure *owner_server() = 0;
};


}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
