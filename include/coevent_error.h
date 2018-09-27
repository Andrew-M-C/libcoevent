// file encoding: UTF-8

#ifndef __CO_EVENT_ERROR_H__
#define __CO_EVENT_ERROR_H__

#include "coevent_const.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <string>

namespace andrewmc {
namespace libcoevent {

// error codes
typedef enum {
    ERR_SUCCESS = 0,
    ERR_EVENT_BASE_NEW,
    ERR_EVENT_BASE_DISPATCH,
    ERR_EVENT_EVENT_NEW,
    ERR_EVENT_BASE_NO_EVENT_PANDING,
    ERR_EVENT_UNEXPECTED_ERROR,

    ERR_LIBCO_CREATE,
    ERR_PARA_NULL,
    ERR_PARA_ILLEGAL,

    ERR_NETWORK_TYPE_ILLEGAL,
    ERR_BIND_PATH_ILLEGAL,

    ERR_INTERRUPTED_SLEEP,
    ERR_TIMEOUT,
    ERR_SIGNAL,

    ERR_NOT_INITIALIZED,
    ERR_OBJ_NOT_FOUND,
    ERR_ALREADY_CONNECTED,
    ERR_NOT_CONNECTED,

    ERR_DNS_SERVER_IP_NOT_FOUND,

    ERR_UNKNOWN     // should place at last
} ErrCode_t;


// libcoevent use this structure to return error information
struct Error {
private:
    uint16_t _sys_errno;    // error defined in "errno.h"
    uint16_t _lib_errno;    // error defined by libcoevent in coevent_const.h
    uint16_t _mysql_errno;  // error defined in MariaDB
    uint16_t _reserved;
    const char *_err_msg;

public:
    Error():
        _sys_errno(0),
        _lib_errno(0),
        _mysql_errno(0),
        _reserved(0),
        _err_msg(NULL)
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

    void set_mysql_errno(int mysql_errno, const char *c_err_msg);

    const char *c_err_msg();
    uint64_t err_code();
    uint32_t app_err_code(void);
    uint32_t sys_err_code(void);
    uint32_t mysql_err_code(void);
};


}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
