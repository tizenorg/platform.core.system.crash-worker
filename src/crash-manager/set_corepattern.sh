#!/bin/sh

/usr/bin/echo "|/usr/bin/crash-manager.sh %p %u %g %s %t %e" > /proc/sys/kernel/core_pattern
