// file encoding: UTF-8

#ifndef __CO_EVENT_VIRTUAL_H__
#define __CO_EVENT_VIRTUAL_H__

#include "coevent_const.h"
#include "coevent_error.h"

#include "event2/event.h"
#include "co_routine.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <set>

namespace andrewmc {
namespace libcoevent {

class Event;
class Client;

class UDPClient;
class DNSClient;
class TCPClient;

class UDPSession;
class TCPSession;

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


}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
