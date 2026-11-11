#!/bin/sh -e

args="$*"
if [ -z "$args" ]; then
	args="cly"
fi

exec valgrind --leak-check=full \
              --show-leak-kinds=all \
              --track-origins=yes \
              --error-limit=no \
              --suppressions=cly_valgrind_memcheck.supp \
              $args
