#!/usr/bin/python3
# -*- coding: UTF-8 -*-

import platform
import os
import sys

from tools import log


def main():
    # "main process"
    log.debug('Hello libcoevent UDP!')
    return


if __name__ == '__main__':
    # sys.settrace(trace)
    main()
    sys.exit(0)
