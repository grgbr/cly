#!/bin/sh -e

exec valgrind --leak-check=full \
              --show-leak-kinds=all \
              --error-limit=no \
              --suppressions=cly_valgrind_memcheck.supp \
              cly
