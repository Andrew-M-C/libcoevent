
#include "coevent.h"


// ==========
#define __CO_EVENT_ERROR
#if 1

void andrewmc::libcoevent::set_sys_errno(uint16_t sys_errno)
{
    _sys_errno = sys_errno;
    _lib_errno = 0;
    return;
}


void andrewmc::libcoevent::set_app_errno(uint16_t lib_errno, const char *c_err_msg)
{
    if (NULL == c_err_msg) {
        c_err_msg = "";
    }

    _sys_errno = 0xFFFF;
    _lib_errno = lib_errno;
    _err_msg = c_err_msg;
    return;
}


void andrewmc::libcoevent::set_app_errno(uint16_t lib_errno, const std::string &err_msg)
{
    _sys_errno = 0xFFFF;
    _lib_errno = lib_errno;
    _err_msg = err_msg;
    return;
}


const char *andrewmc::libcoevent::get_c_err_msg()
{
    return _err_msg.c_str();
}


uint32_t andrewmc::libcoevent::get_err_code()
{
    uint32_t ret = (uint32_t)_sys_errno;
    ret += ((uint32_t)_lib_errno) << 16;
    return ret;
}

#endif  // end of CoEventError

