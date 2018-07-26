#!/usr/bin/python3
# -*- coding: UTF-8 -*-

import socket
import sys

from tools import log


def main():
    # "main process"
    log.debug('Hello libcoevent TCP!')
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    msg = 'www.google.com'
    if len(sys.argv) > 1:
        msg = str(sys.argv[1])

    sock.settimeout(2.0)
    sock.connect(('127.0.0.1', 6666))

    log.debug('parameter: %s' % msg)
    sock.send(msg.encode())
    log.debug('Data sent')

    if msg != 'quit':
        data = sock.recv(2048)
        log.debug('Got reply: "%s"' % (data.decode(),))
    return


if __name__ == '__main__':
    # sys.settrace(trace)
    ret = main()

    if ret is None:
        ret = 0
    sys.exit(ret)
