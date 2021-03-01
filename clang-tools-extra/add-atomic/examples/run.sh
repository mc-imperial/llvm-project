#!/bin/bash

set -e

rm -rf temp
mkdir temp

for f in `ls *.c`
do
    echo "Running $f"
    $1/add-atomic $f -name target -o temp/upgraded.c -- &> /dev/null
    echo "  add-atomic succeded"
    diff temp/upgraded.c expected/$f
    echo "  results match"
    clang-11 -c temp/upgraded.c -Wall -Werror -o temp/upgraded.o
done

echo "SUCCESS!"
rm -rf temp
