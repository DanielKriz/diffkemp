# rm -rf build/
# rm -rf diffkemp/simpll/_simpll.abi3.so
rm -rf build/diffkemp/simpll/libsimpll-lib.a
cmake -GNinja -Bbuild -DCMAKE_BUILD_TYPE=Debug -DSIMPLL_REBUILD_BINDINGS=1
cmake --build build
pip install -e .
