# metasweep

**Local. Transparent. Fast.**

## Build
```bash
sudo apt update
sudo apt install cmake build-essential libexiv2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Quickstart

```bash
./build/metasweep inspect path/to/photo.jpg
./build/metasweep strip path/to/photo.jpg           # -> photo.cleaned.jpg
./build/metasweep strip --safe path/to/photo.jpg
```