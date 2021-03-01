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
done

rm -rf temp
