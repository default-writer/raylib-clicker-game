#!/bin/bash
set -e

gcc main.c -o ./main -lraylib -lsqlite3 -lGL -lm -lpthread -ldl -lrt -lX11

./main