# hwckr

hwckr is a Linux hardware/system information utility written in C++ with planned low-level Assembly support.

## Goals
- Display CPU, memory, storage, and system information
- Show live hardware/system metrics
- Add low-level CPU detection using Assembly
- Build a clean native Linux desktop UI with Qt

## Stack
- C++
- Qt6
- NASM (planned)
- CMake

## Build
```bash
cd build
cmake ..
make -j$(nproc)
./hwckr
