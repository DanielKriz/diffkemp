#!/bin/bash

if [ "$1" = "debug" ]; then
    gdb --ex=r --args ./build/diffkemp/simpll/diffkemp-simpll pattern --fun=add Runtime/test1/main.ll
elif [ "$1" = "test" ]; then
    ./tests/unit_tests/simpll/runTests
elif [ "$1" = "conf" ]; then
    ./bin/diffkemp generate -c Runtime/pattern_config.yaml
else
    ./bin/diffkemp generate -f add Runtime/test1/main.ll Runtime/test2/main.ll
fi
