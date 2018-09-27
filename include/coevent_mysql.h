// file encoding: UTF-8

#ifndef __CO_EVENT_MYSQL_H__
#define __CO_EVENT_MYSQL_H__

#include "coevent_error.h"
#include "coevent_const.h"
#include "coevent_virtual.h"

#include "co_routine.h"
#include <event2/event.h>
#include <mysql.h>

namespace andrewmc {
namespace libcoevent {

class MySQLClient : public Client {
protected:
    std::string     _address;
    std::string     _unix_address;
    unsigned        _port;
    std::string     _database;
    std::string     _user;
    std::string     _password;
    MYSQL           _mysql;

    void _set_mysql_err();

public:
    MySQLClient();
    virtual ~MySQLClient(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error connect_DB(const char *address = NULL,
                                    const char *user = NULL,
                                    const char *password = NULL,
                                    const char *database = NULL,
                                    unsigned port = 0,
                                    BOOL unix_socket = FALSE) = 0;

    virtual struct Error query(const char *query, size_t length = 0xFFFFFFFF) = 0;
    virtual MYSQL_RES *use_result();                    // non-block function
    virtual MYSQL_RES *store_result() = 0;
    virtual MYSQL_ROW *fetch_row(MYSQL_RES *result) = 0;
    virtual unsigned num_fields(MYSQL_RES *result);     // non-block function
    virtual struct Error free_result(MYSQL_RES *result) = 0;
    virtual struct Error close() = 0;

    virtual struct timeval timeout_timeval(void) const = 0;
    virtual double timeout(void) const = 0;

    // get functions
    const std::string &address() const {return _address;};
    const std::string &unix_socket() const {return _unix_address;};
    const std::string &database() const {return _database;};
    const std::string &user() const {return _user;};
    const std::string &password() const {return _password;};
    unsigned port() const {return _port;};

    // set functions
    void set_address(const std::string &address) {_address = address;};
    void set_unix_socket(const std::string &unix_socket) {_unix_address = unix_socket;};
    void set_database(const std::string &database) {_database = database;};
    void set_user(const std::string &user) {_user = user;};
    void set_password(const std::string &password) {_password = password;};
    void set_port(unsigned port) {_port = port & 0x7FFFFFFF;};
};

}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
