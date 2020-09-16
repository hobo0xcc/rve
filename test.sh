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
        cd test
        make clean &> /dev/null
        cd ..
        exit 1
    fi
}

cd test
make clean &> /dev/null
cd ..
