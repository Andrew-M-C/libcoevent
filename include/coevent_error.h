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


}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
