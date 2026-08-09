# Setup Guide

## Small intro

This project is written in **standard C++17** and uses **CMake** as the only build system.

The allocator code is not Mac-only. It should work on:

- macOS
- Windows
- Linux

Once the required tools are installed, everyone should be able to build the project using the same basic CMake commands.

---

## macOS setup

### Install C++ compiler

Install Apple Command Line Tools:

```bash
xcode-select --install
```

Check compiler:

```bash
c++ --version
```

### Install CMake

If you have Homebrew:

```bash
brew install cmake
```

Check CMake:

```bash
cmake --version
```

---

## Windows setup

Recommended option: install **Visual Studio Community**.

During installation, select:

```text
Desktop development with C++
```

This installs:

- MSVC C++ compiler
- CMake support
- Windows build tools

Check from **Developer PowerShell for Visual Studio**:

```powershell
cmake --version
```

Alternative option: use **WSL Ubuntu** and follow the Linux setup.

---

## Linux setup

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake
```

Check:

```bash
g++ --version
cmake --version
```

### Fedora

```bash
sudo dnf install gcc-c++ cmake
```

Check:

```bash
g++ --version
cmake --version
```

---

## Build and run

From the project root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run the demo:

### macOS/Linux

```bash
./build/allocator_demo
```

### Windows

Debug build:

```powershell
.\build\Debug\allocator_demo.exe
```

Release build:

```powershell
.\build\Release\allocator_demo.exe
```

---

## Running tests

### macOS/Linux

```bash
./build/allocator_tests
```

### Windows

```powershell
.\build\Release\allocator_tests.exe
```

or, if built in Debug mode:

```powershell
.\build\Debug\allocator_tests.exe
```

---

## Running benchmarks

### macOS/Linux

Simple output:

```bash
./build/benchmark_allocators
```

Detailed output:

```bash
./build/benchmark_allocators --detailed
```

### Windows

```powershell
.\build\Release\benchmark_allocators.exe
```

or, if built in Debug mode:

```powershell
.\build\Debug\benchmark_allocators.exe
```

For meaningful benchmark numbers, always prefer a Release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Benchmark files are written to:

```text
benchmark_results/results.csv
benchmark_results/results.txt
```

---

## Plotting benchmark graphs

Install Python graph dependency:

```bash
python3 -m pip install -r requirements.txt
```

Generate PNG graphs:

```bash
python3 tools/plot_benchmarks.py
```

If you use the project virtual environment:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python tools/plot_benchmarks.py
```

Graphs are saved under:

```text
results/
```
