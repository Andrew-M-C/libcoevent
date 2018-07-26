
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

typedef Event _super;

// ==========
// public functions
#define __PUBLIC_FUNCTIONS
#ifdef __PUBLIC_FUNCTIONS

UDPClient *Procedure::new_UDP_client(NetType_t network_type, void *user_arg)
{
    if (NULL == _coroutine()) {
        return NULL;
    }

    UDPItnlClient *client = new UDPItnlClient;
    Error status = client->init(this, _coroutine(), network_type, user_arg);

    if (status.is_ok()) {
        _client_chain.insert(client);
    }
    else {
        ERROR("Failed to init UDP client: %s", status.c_err_msg());
        delete client;
        client = NULL;
    }
    return (UDPClient *)client;
}


DNSClient *Procedure::new_DNS_client(NetType_t network_type, void *user_arg)
{
    if (NULL == _coroutine()) {
        return NULL;
    }

    DNSItnlClient *client = new DNSItnlClient;
    Error status = client->init(this, _coroutine(), network_type, user_arg);

    if (status.is_ok()) {
        _client_chain.insert(client);
    }
    else {
        ERROR("Failed to init DNS client: %s", status.c_err_msg());
        delete client;
        client = NULL;
    }
    return (DNSClient *)client;
}


TCPClient *Procedure::new_TCP_client(NetType_t network_type, void *user_arg)
{
    if (NULL == _coroutine()) {
        return NULL;
    }

    TCPItnlClient *client = new TCPItnlClient;
    Error status = client->init(this, _coroutine(), network_type, user_arg);

    if (status.is_ok()) {
        _client_chain.insert(client);
    }
    else {
        ERROR("Failed to init TCP client: %s", status.c_err_msg());
        delete client;
        client = NULL;
    }
    return (TCPClient *)client;
}


struct Error Procedure::delete_client(Client *client)
{
    if (NULL == client) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    std::set<Client *>::iterator it = _client_chain.find(client);
    if (it != _client_chain.end())
    {
        _status.clear_err();
        DEBUG("Delete client '%s' from '%s'", client->identifier().c_str(), _identifier.c_str());
        owner()->delete_event_under_control(*it);
        _client_chain.erase(client);
    }
    else {
        DEBUG("Client '%s' is not in '%s'", client->identifier().c_str(), _identifier.c_str());
        _status.set_app_errno(ERR_OBJ_NOT_FOUND);
    }

    return _status;
}


Procedure::~Procedure()
{
    DEBUG("Delete procedure client chain of %s", _identifier.c_str());

    // free all clients under control
    for (std::set<Client *>::iterator it = _client_chain.begin(); 
        it != _client_chain.end(); 
        it ++)
    {
        owner()->delete_event_under_control(*it);
    }
    _client_chain.clear();

    return;
}


struct stCoRoutine_t *Procedure::_coroutine()
{
    return NULL;
}


#endif


// end of file
