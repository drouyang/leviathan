#!/bin/sh

if [ -e leviathan.pid ]; then
    kill -s INT $(cat leviathan.pid)
else
    kill -s INT $(pidof lnx_init)
fi
