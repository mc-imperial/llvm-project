#!/bin/bash

set -e
set -x

rm -rf temp
mkdir temp

for i in `seq 1 100`
do
    echo "Seed $i"
    echo " Running csmith"
    $1/build/src/csmith --seed $i -o temp/prog.c
    echo " Running add-atomic"
    $2/add-atomic temp/prog.c -o temp/upgraded.c -seed $i -- -I $1/runtime -I $1/build/runtime -I /usr/lib/llvm-11/lib/clang/11.0.0/include > /dev/null
    gcc -Werror=incompatible-pointer-types -I $1/runtime -I $1/build/runtime temp/upgraded.c
done

echo "SUCCESS!"
rm -rf temp
