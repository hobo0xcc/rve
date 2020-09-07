#!/bin/bash

make &> /dev/null

cd test
make &> /dev/null
cd ..

run_test() {
    ./bin/rve "test/$1"
    code="$?"
    if [ "$code" = "$2" ]; then
        echo "$1: Ok"
    else
        echo "$1: Failed ... expected $2 but got $code"
        exit 1
    fi
}

run_test addi-1.bin 50
run_test addi-2.bin 0
run_test slti-1.bin 1
run_test sltiu-1.bin 1
run_test xori-1.bin 3
run_test ori-1.bin 3
run_test andi-1.bin 3

cd test
make clean &> /dev/null
cd ..
