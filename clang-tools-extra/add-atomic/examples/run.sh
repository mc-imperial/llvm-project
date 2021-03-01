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
    gcc -c expected/$f -Werror=incompatible-pointer-types -o temp/$f.o
done

rm -rf temp
