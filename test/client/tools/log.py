#!/usr/bin/python3
# -*- coding: UTF-8 -*-
# Reference:
#   [如何获取Python中最后一部分路径？](https://codeday.me/bug/20170616/27046.html)

import traceback
# import time
import datetime
# import os

_enable_log_func = True
_enable_log_line = True
_enable_log_file = True
_enable_log_statement = False
_log_identifier = ''


LOG_EMERG = 0       # 0: system is unusable
LOG_ALERT = 1       # 1: action must be taken immediately
LOG_CRIT = 2        # 2: critical conditions
LOG_ERROR = 3       # 3: error conditions
LOG_ERR = 3         # 3: error conditions
LOG_WARNING = 4     # 4: warning conditions
LOG_WARN = 4        # 4: warning conditions
LOG_NOTICE = 5      # 5: normal, but significant, condition
LOG_INFO = 6        # 6: informational message
LOG_DEBUG = 7       # 7: debug-level message


console_level = LOG_DEBUG   # log level


def enable_log_function(flag):
    'enable/disable function name in logs'
    global _enable_log_func
    if flag:
        _enable_log_func = True
    else:
        _enable_log_func = False
    return


def enable_log_line(flag):
    'enable/disable line info in logs'
    global _enable_log_line
    global _enable_log_file
    if flag:
        _enable_log_line = True
        _enable_log_file = True
    else:
        _enable_log_line = False
    return


def enable_log_file(flag):
    'enable/disable file info in logs'
    global _enable_log_file
    if flag:
        _enable_log_file = True
    else:
        _enable_log_file = False
    return


def enable_enable_log_statement(flag):
    'enable/disable caller statement in logs'
    global _enable_log_statement
    if flag:
        _enable_log_statement = True
    else:
        _enable_log_statement = False
    return


def _pack_msg(level, stack, message):
    '打包组装最终的字符串'
    log_parts = []

    # time information
    # part = '[%s]' % time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    part = '[%s]' % datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')
    log_parts.append(part)

    # file information
    if _enable_log_file:
        file_dir_and_name = str(stack.filename).split('/')
        if _enable_log_line:
            part = ' [%s, Line %d' % (file_dir_and_name[-1], stack.lineno)
        else:
            part = ' [%s' % file_dir_and_name[-1]
        # log_parts.append(part)

        if _enable_log_func:
            part += ', %s()]' % stack.name
        else:
            part += ']'

    elif _enable_log_func:
        part = ' [%s()]' % stack.name
        log_parts.append(part)

    # log level
    if LOG_DEBUG == level:
        part = ' [DEBUG]  -   '
    elif LOG_INFO == level:
        part = ' [INFO ] (:..)'
    elif LOG_NOTICE == level:
        part = ' [NOTCE] (:.:)'
    elif LOG_WARNING == level:
        part = ' [WARN ] (!.!)'
    elif LOG_ERR == level:
        part = ' [ERROR] (!!!)'
    elif LOG_CRIT == level:
        part = ' [CRIT ] (X!!)'
    elif LOG_ALERT == level:
        part = ' [ALERT] (XX!)'
    elif LOG_EMERG == level:
        part = ' [EMERG] (XXX)'
    else:
        part = ' [DEBUG]'
    log_parts.append(part)

    # log context
    log_parts.append(' - ')

    if len(_log_identifier) > 0:
        log_parts.append('%s: ' % _log_identifier)

    log_parts.append(message)

    # caller statement
    if _enable_log_statement:
        part = ' <caller: "%s">' % stack.line
        log_parts.append(part)

    return ''.join(log_parts)


def log(log_level, msg, stack=None):
    '输出 log'
    if stack is None:
        stack_list = traceback.extract_stack()
        stack = stack_list[-2]

    final_msg = _pack_msg(log_level, stack, msg)
    ret = len(final_msg)

    # print to console
    if log_level <= console_level:
        print(final_msg)

    # 返回
    return ret


def mark():
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_DEBUG, '<<< MARK >>>', last_stack)


def debug(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_DEBUG, msg, last_stack)


def info(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_INFO, msg, last_stack)


def notice(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_NOTICE, msg, last_stack)


def warn(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_WARN, msg, last_stack)


def error(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_ERROR, msg, last_stack)


def critical(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_CRIT, msg, last_stack)


def alert(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_ALERT, msg, last_stack)


def emerge(msg):
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return log(LOG_EMERG, msg, last_stack)


def function_name():
    stack = traceback.extract_stack()
    last_stack = stack[-2]
    return last_stack.name


def caller_name():
    stack = traceback.extract_stack()
    last_stack = stack[-3]
    return last_stack.name


def caller_file():
    stack = traceback.extract_stack()
    last_stack = stack[-3]
    return last_stack.filename


def set_log_identifier(id):
    global _log_identifier
    _log_identifier = str(id)
    return
