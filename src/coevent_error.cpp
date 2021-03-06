
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
    "unexpected error in libevent",

    "failed to create libco routine",
    "necessary parameter is null",
    "parameter illegal",

    "illegal network interface type",
    "ilegal bind path",

    "sleep action is interrupted",
    "timeout",
    "interrupted by signal",

    "object is not initialized",
    "object not found",
    "TCP already connected",
    "TCP not connected",

    "cannot find approperate DNS server IP",

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


void Error::clear_err()
{
    set_sys_errno(0);
    return;
}


void Error::set_sys_errno()
{
    _sys_errno = (uint16_t)errno;
    _lib_errno = 0;
    return;
}


void Error::set_sys_errno(int sys_errno)
{
    _sys_errno = (uint16_t)sys_errno;
    _lib_errno = 0;
    return;
}


void Error::set_app_errno(ErrCode_t lib_errno, const char *c_err_msg)
{
    if (NULL == c_err_msg) {
        c_err_msg = "";
    }

    _sys_errno = (0 == lib_errno) ? 0 : 0xFFFF;
    _lib_errno = (uint16_t)lib_errno;

    if ((NULL == c_err_msg)
        || ('\0' == *c_err_msg))
    {
        if (lib_errno < ERR_UNKNOWN) {
            _err_msg = g_error_msg_list[lib_errno];
        }
        else {
            _err_msg = "unknown error code";
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
    else if (_err_msg) {
        return _err_msg;
    }
    else {
        return g_error_msg_list[0];
    }
}


uint32_t Error::err_code()
{
    uint32_t ret = (uint32_t)_sys_errno;
    ret += ((uint32_t)_lib_errno) << 16;
    return ret;
}


uint32_t Error::app_err_code()
{
    return _lib_errno;
}


uint32_t Error::sys_err_code()
{
    return _sys_errno;
}

#endif  // end of Error

