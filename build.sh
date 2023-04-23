# rm -rf build/
rm -f diffkemp/simpll/_simpll.abi3.so
rm -f diffkemp/simpll/_simpll.abi3.o
rm -f diffkemp/simpll/_simpll.cpython-38-x86_64-linux-gnu.so
rm -f diffkemp/simpll/_simpll.abi3.o
rm -f build/diffkemp/simpll/libsimpll-lib.a
rm -rf build/diffkemp/building
rm -rf build/lib.linux-x86_64-3.8
cmake -GNinja -Bbuild -DCMAKE_BUILD_TYPE=Debug -DSIMPLL_REBUILD_BINDINGS=1
cmake --build build
./diffkemp/simpll/simpll_build.py
pip install -e .
