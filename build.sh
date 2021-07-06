#!/bin/sh

set -e

case "$1" in
"")
    gcc wavdev.c fft.c sounddet.c -D_TEST_ -Wall -lwinmm -o sounddet
    ;;
clean)
    rm -rf *.exe
    ;;
esac
