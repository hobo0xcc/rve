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

run_test addi-1.bin 50
run_test addi-2.bin 0
run_test slti-1.bin 1
run_test sltiu-1.bin 1
run_test xori-1.bin 3
run_test ori-1.bin 3
run_test andi-1.bin 3
run_test add-1.bin 40
run_test sub-1.bin 20
run_test sll-1.bin 16
run_test slt-1.bin 1
run_test sltu-1.bin 1
run_test xor-1.bin 3
run_test srl-1.bin 8
run_test sra-1.bin 8
run_test slli-1.bin 32
run_test srli-1.bin 8
run_test srai-1.bin 8
run_test nop-1.bin 0
run_test jal-1.bin 4 
run_test jalr-1.bin 8
run_test lb-1.bin 20
run_test beq-1.bin 42

cd test
make clean &> /dev/null
cd ..
