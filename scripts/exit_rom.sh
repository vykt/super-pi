#!/bin/sh

SNES9X_PID=$(pidof snes9x)
XORG_PID=$(pidof Xorg)

# terminate snes9x
kill -SIGTERM SNES9X_PID
while kill -0 $SNES9X_PID 2> /dev/null; do
  usleep 200000
done

# terminate Xorg
kill -SIGTERM XORG_PID
while kill -0 $XORG_PID 2> /dev/null; do
  usleep 200000
done
