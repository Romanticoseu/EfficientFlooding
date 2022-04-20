#!/usr/bin/env bash
echo "Flasing 1"
CLOAD_CMDS="-w radio://0/1/2M/E7E7E7E7E7" make cload
echo "Flasing 2"
CLOAD_CMDS="-w radio://0/2/2M/E7E7E7E7E7" make cload
echo "Flasing 3"
CLOAD_CMDS="-w radio://0/3/2M/E7E7E7E7E7" make cload