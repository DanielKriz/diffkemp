rm -rf build/
cmake -GNinja -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
pip install -e .
