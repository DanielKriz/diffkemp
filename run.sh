#!/bin/bash

if [ "$1" = "debug" ]; then
    gdb --ex=r --args ./build/diffkemp/simpll/diffkemp-simpll pattern --fun=add Runtime/test1/main.ll
elif [ "$1" = "test" ]; then
    ./tests/unit_tests/simpll/runTests
else
    ./bin/diffkemp generate -f add Runtime/test1/main.ll Runtime/test2/main.ll
fi
