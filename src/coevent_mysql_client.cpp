
#if ENABLE_MARIADB

#include "coevent.h"
#include "coevent_itnl.h"
#include "coevent_mysql.h"
#include <string.h>
#include <string>
#include <stdlib.h>

using namespace andrewmc::libcoevent;

#define _ALLOC_ERROR        (0xFFFFFFFF)

// ==========
// necessary definitions
#define __CO_EVENT_MYSQL_CLIENT_DEFINITIONS
#ifdef __CO_EVENT_MYSQL_CLIENT_DEFINITIONS

struct _EventArg {
    MySQLItnlClient     *event;
    uint32_t            *libevent_what_ptr;

    struct stCoRoutine_t *coroutine;

    _EventArg(): event(NULL), libevent_what_ptr(NULL), coroutine(NULL)
    {}
};

#endif


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __MYSQL_CLIENT_SERVER_CALLBACK
#ifdef __MYSQL_CLIENT_SERVER_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;
    MySQLItnlClient *client = arg->event;
    Procedure *server = client->owner_server();
    Base *base = client->owner();

    // switch into the coroutine
    if (arg->libevent_what_ptr) {
        *(arg->libevent_what_ptr) = (uint32_t)what;
        DEBUG("libevent what: 0x%08x - %s%s", (unsigned)what, event_is_timeout(what) ? "timeout " : "", event_readable(what) ? "read" : "");
    }
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the event if this is under control of the base
        DEBUG("Server %s ends", server->identifier().c_str());
        base->delete_event_under_control(server);
    }

    // done
    return;
}

#endif


// ==========
// MySQLClient class functions
#define __MYSQL_CLIENT_CLASS_FUNCTIONS
#ifdef __MYSQL_CLIENT_CLASS_FUNCTIONS

MySQLClient::MySQLClient()
{
    _port = 3306;
    MYSQL *mysql_ret = mysql_init(&_mysql);
    if (NULL == mysql_ret)
    {
        throw std::bad_alloc();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        _port = _ALLOC_ERROR;
        return;
    }

    mysql_options(&_mysql, MYSQL_OPT_NONBLOCK, 0);
    return;
}


void MySQLClient::_set_mysql_err()
{
    _status.set_mysql_errno(mysql_errno(&_mysql), mysql_error(&_mysql));
    return;
}


MYSQL_RES *MySQLClient::use_result()
{
    MYSQL_RES *ret = mysql_use_result(&_mysql);
    _set_mysql_err();
    return ret;
}


unsigned MySQLClient::num_fields(MYSQL_RES *result)
{
    return (unsigned)mysql_num_fields(result);
}

#endif  // end of __MYSQL_CLIENT_CLASS_FUNCTIONS


// ==========
// other internal functions
#define __OTHER_INTERNAL_FUNCTIONS
#ifdef __OTHER_INTERNAL_FUNCTIONS

static int _mysql_status_from_libevent(short event)
{
    int status = 0;
    if (event & EV_READ)
        status |= MYSQL_WAIT_READ;
    if (event & EV_WRITE)
        status |= MYSQL_WAIT_WRITE;
    if (event & EV_TIMEOUT)
        status |= MYSQL_WAIT_TIMEOUT;
    return status;
}


static short _libevent_events_from_mysql(int status)
{
    short event = 0;
    if (status & MYSQL_WAIT_READ)
        event |= EV_READ;
    if (status & MYSQL_WAIT_WRITE)
        event |= EV_WRITE;
    return event;
}


#endif  // end of __OTHER_INTERNAL_FUNCTIONS


// ==========
// constructors and destructors
#define __CONSTRUCTORS_AND_DESRUCTORS
#ifdef __CONSTRUCTORS_AND_DESRUCTORS

MySQLItnlClient::MySQLItnlClient()
{
    _is_connected = FALSE;
    _event_arg = NULL;
    _owner_server = NULL;
    _libevent_what_storage = NULL;

    _libevent_what_storage = (uint32_t *)malloc(sizeof(*_libevent_what_storage));
    if (NULL == _libevent_what_storage)
    {
        throw(std::bad_alloc());
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        _port = _ALLOC_ERROR;
        return;
    }

    struct _EventArg *arg = (struct _EventArg *)malloc(sizeof(*arg));
    if (NULL == _arg) {
        throw(std::bad_alloc());
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        _port = _ALLOC_ERROR;
        return;
    }

    _status.clear_err();
    _event_arg = arg;
    arg->event = this;
    arg->libevent_what_ptr = _libevent_what_storage;

    return;
}


MySQLItnlClient::~MySQLItnlClient()
{
    if (_is_connected) {
        this->close();
    }

    if (_libevent_what_storage) {
        free(_libevent_what_storage);
        _libevent_what_storage = NULL;
    }

    if (_arg) {
        free(_arg);
        _arg = NULL;
    }

     // TODO:
    return;
}


struct Error MySQLItnlClient::init(Procedure *server, struct stCoRoutine_t *coroutine)
{
    if (!(server && coroutine)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    if (NULL == _libevent_what_storage
        || NULL == _event_arg)
    {
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }

    _status.clear_err();
    struct _EventArg *arg = (struct _EventArg *)_event_arg;
    arg->coroutine = coroutine;

    return _status;
}


Procedure *MySQLItnlClient::owner_server()
{
    return _owner_server;
}


#endif  // end of __CONSTRUCTORS_AND_DESRUCTORS


// ==========
#define __MYSQL_PROCESS_FUNCTIONS
#ifdef __MYSQL_PROCESS_FUNCTIONS

struct Error MySQLItnlClient::connect_DB(const char *address, const char *user, const char *password, const char *database, unsigned port, BOOL unix_socket)
{
    // TODO:
    return _status;
}


#endif  // end of __MYSQL_PROCESS_FUNCTIONS

#endif  // end of ENABLE_MARIADB
// end of file;
