#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# A resident route without a Loader witness must fail the construction gate.
if [ "$1" = "--list-routes" ]; then
    printf 'alpha\nbeta\n'
    exit 0
fi
case "$DBUS_SESSION_BUS_ADDRESS" in
    *absent-session-bus) ;;
    *) exit 88 ;;
esac
case "$DBUS_SYSTEM_BUS_ADDRESS" in
    *absent-system-bus) ;;
    *) exit 89 ;;
esac
route=
while [ "$#" -gt 0 ]; do
    if [ "$1" = "--page" ]; then
        route=$2
        shift
    fi
    shift
done
if [ "$route" = "alpha" ]; then
    printf 'QINDAQT_ROUTE_CONSTRUCTED alpha ready\n'
    if [ "$FAKE_ROUTE_WARN" = 1 ]; then
        printf 'qrc:/fake/Alpha.qml:7: TypeError: failed binding\n' >&2
    fi
    exit 0
fi
if [ "$route" = "beta" ]; then
    sleep 3
    exit 0
fi
exit 90
