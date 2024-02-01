### Fengge::ARC

Fengge::ARC is an ARC (A Self-Tuning, Low Overhead Replacement Cache) implementation using pure C++ STL.
It is C++ header only.

### How to install

```
mkdir build
cd build
cmake ..
make
make install
```

### How to link it
If you use cmake as build tool for your application, just put following lines in you CMakeLists.txt:

```
find_package(FenggeARC)
target_link_libraries([your target] Fengge::fengge_arc)
```

The target Fengge::fengge_arc defines interface include path and will be automatically used by CMake.

