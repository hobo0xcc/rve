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

run_test_isa() {
    files=${1}
    failed=()
    for file in $files ; do
        ./bin/rve --debug "$file" &> /dev/null
        if [ "$?" != "0" ]; then
            echo "$file: fail"
            failed+=($file)
        else
            echo "$file: pass"
        fi
    done

    # for f in ${failed[@]}; do
    #     echo "${f}"
    # done

    if [ "${#failed[*]}" = "0" ]; then
        echo "All tests passed! :)"
    else
        echo "${#failed[*]} test(s) failed... :("
    fi
}

run_test_isa "`find ./test/riscv-tests -not -name "*.dump" | grep rv64ui-p-`"
run_test_isa "`find ./test/riscv-tests -not -name "*.dump" | grep rv64um-p-`"
run_test_isa "`find ./test/riscv-tests -not -name "*.dump" | grep rv64ua-p-`"
run_test_isa "`find ./test/riscv-tests -not -name "*.dump" | grep rv64uc-p-`"

cd test
make clean &> /dev/null
cd ..
