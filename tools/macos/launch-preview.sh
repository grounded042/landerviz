#!/bin/sh

if [ "$#" -eq 0 ]; then
  exec @preview@ --demo
fi

exec @preview@ "$@"
