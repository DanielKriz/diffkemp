rm -rf build/
# rm -rf diffkemp/simpll/_simpll.abi3.so
cmake -GNinja -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
pip install -e .
