
// libevent man page: http://www.wangafu.net/~nickm/libevent-2.1/doxygen/html/

#include "coevent.h"
#include <errno.h>
#include <string.h>
#include <string>

using namespace andrewmc::libcoevent;

// ==========
#define __CO_EVENT_ERROR_GLOBAL_PARA
#if 1

const char *g_error_msg_list[] = {
    "success",
    "failed to create libevent base",
    "failed to dispatch libevent base",
    "failed to dispatch libevent event",
    "no events were pending or active",

    "failed to create libco routine",
    "necessary parameter is null",

    "illegal network interface type",
    "ilegal bind path",

    "sleep action is interrupted",
    "timeout",

    "object is not initialized",
    "object not found",

    "unknown error"     // should place at last
};

#endif  // end of global para


// ==========
#define __CO_EVENT_ERROR
#if 1

BOOL Error::is_error()
{
    return (0 != _sys_errno);
}


BOOL Error::is_ok()
{
    return (0 == _sys_errno);
}


BOOL Error::is_OK()
{
    return (0 == _sys_errno);
}


BOOL Error::is_timeout()
{
    return (ERR_TIMEOUT == _lib_errno);
}


void Error::set_ssize_t(ssize_t val)
{
    if (val >= 0) {
        set_sys_errno(0);
        _ssize_ret = val;
    }
    else {
        set_sys_errno((int)val);
    }
}


ssize_t Error::ssize()
{
    return _ssize_ret;
}


void Error::clear_err()
{
    set_sys_errno(0);
    return;
}


void Error::set_sys_errno()
{
    _sys_errno = (uint16_t)errno;
    _lib_errno = 0;
    _ssize_ret = 0;
    _err_msg.clear();
    return;
}


void Error::set_sys_errno(int sys_errno)
{
    _sys_errno = (uint16_t)sys_errno;
    _lib_errno = 0;
    _ssize_ret = 0;
    _err_msg.clear();
    return;
}


void Error::set_app_errno(ErrCode_t lib_errno, const char *c_err_msg)
{
    if (NULL == c_err_msg) {
        c_err_msg = "";
    }

    _sys_errno = (0 == lib_errno) ? 0 : 0xFFFF;
    _lib_errno = (uint16_t)lib_errno;
    _ssize_ret = 0;

    if ((NULL == c_err_msg)
        || ('\0' == *c_err_msg))
    {
        if (lib_errno < ERR_UNKNOWN) {
            _err_msg = g_error_msg_list[lib_errno];
        }
        else {
            char msg[32];
            sprintf(msg, "unknown error code %u", (unsigned)lib_errno);
            _err_msg = msg;
        }
    }
    else {
        _err_msg = c_err_msg;
    }
    return;
}


void Error::set_app_errno(ErrCode_t lib_errno, const std::string &err_msg)
{
    set_app_errno(lib_errno, err_msg.c_str());
    return;
}


void Error::set_app_errno(ErrCode_t lib_errno)
{
    set_app_errno(lib_errno, "");
    return;
}


const char *Error::c_err_msg()
{
    if (_sys_errno != 0xFFFF) {
        return strerror((int)_sys_errno);
    }
    else {
        return _err_msg.c_str();
    }
}


uint32_t Error::err_code()
{
    uint32_t ret = (uint32_t)_sys_errno;
    ret += ((uint32_t)_lib_errno) << 16;
    return ret;
}

#endif  // end of Error

