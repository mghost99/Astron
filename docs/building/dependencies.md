Astron Dependencies
===================

**Last updated: 11/11/2025**

This document lists all dependencies required to build Astron across different platforms.
All platforms use the same minimum dependency versions to ensure consistency.

## Required Dependencies ##

These dependencies are required for all builds of Astron:

| Dependency   | Minimum Version | Purpose                                    |
|--------------|-----------------|------------------------------------------- |
| CMake        | 3.15+           | Build system generator                     |
| C++ Compiler | C++20 support   | GCC, Clang, or MSVC                        |
| Boost        | 1.55+           | C++ utility libraries (mainly ICL)         |
| libyaml-cpp  | 0.5+            | YAML parsing for configuration             |
| libuv        | 1.23.0+         | Cross-platform asynchronous I/O            |
| OpenSSL      | 1.0.1+          | SSL/TLS and cryptography                   |
| Git          | Any             | Version control (for cloning repository)   |
| Bison        | Any             | Parser generator (for dclass files)        |
| Flex         | Any             | Lexical analyzer (for dclass files)        |

## Optional Dependencies ##

### Database Backends ###

**MongoDB:**
- mongo-c-driver 2.1.2+
- mongo-cxx-driver (releases/stable branch)
- libmongocrypt (optional but recommended)
- MongoDB Community Server 8.2+ (for running the database)
- Allows using MongoDB as the database backend

## Platform-Specific Packages ##

### Linux (Ubuntu/Debian 25.10) ###

**Required packages:**
```bash
sudo apt install bison curl flex g++ git gnupg libboost-dev libicu-dev \
    libpq-dev libsasl2-dev libssl-dev libuv1-dev libyaml-cpp-dev \
    libzstd-dev pkg-config
```

**MongoDB packages:**
```bash
sudo apt install -y git build-essential pkg-config libcurl4-openssl-dev \
    libssl-dev libpcap-dev libpcre3-dev libsnappy-dev zlib1g-dev \
    libgoogle-perftools-dev python3 python3-pip python3-venv python-pymongo \
    libreadline-dev libstemmer-dev
```

### macOS (Homebrew) ###

**Required packages:**
```bash
brew install cmake ninja boost libuv yaml-cpp openssl git bison flex
```

**MongoDB packages:**
```bash
brew install mongodb-community curl icu4c pcre snappy zlib zstd
```

### Windows (MSYS2 MinGW64) ###

**Required packages:**
```bash
pacman -S --needed base-devel mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-boost mingw-w64-x86_64-libuv \
    mingw-w64-x86_64-yaml-cpp mingw-w64-x86_64-openssl git
```

**MongoDB packages:**
```bash
pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-icu \
    mingw-w64-x86_64-pcre mingw-w64-x86_64-snappy \
    mingw-w64-x86_64-zlib mingw-w64-x86_64-zstd \
    mingw-w64-x86_64-cyrus-sasl
```

### Windows (Visual Studio + vcpkg) ###

**Recommended: Use vcpkg package manager:**

```cmd
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Install all required dependencies
.\vcpkg install boost:x64-windows libuv:x64-windows yaml-cpp:x64-windows openssl:x64-windows winflexbison:x64-windows

# Integrate with Visual Studio
.\vcpkg integrate install
```

For manual installation or detailed instructions, see [windows-visualstudio.md](windows-visualstudio.md).

## Building MongoDB Drivers ##

MongoDB support requires building the MongoDB C and C++ drivers from source. **Build instructions are platform-specific** due to different toolchains, paths, and quirks.

**For detailed MongoDB build instructions, see your platform-specific guide:**

- **Linux:** See [linux-gnu-make.md](linux-gnu-make.md#building-mongodb-drivers-optional) - Section "Building MongoDB Drivers"
- **macOS:** See [macos-homebrew.md](macos-homebrew.md#building-mongodb-drivers-optional) - Section "Building MongoDB Drivers"
- **Windows (MinGW):** See [windows-mingw.md](windows-mingw.md#4-building-mongodb-drivers-optional) - Section 4: Building MongoDB Drivers
  - **Important:** Windows MinGW requires special flags and DLL considerations
- **Windows (Visual Studio):** See [windows-visualstudio.md](windows-visualstudio.md) - Use vcpkg: `vcpkg install mongo-cxx-driver:x64-windows`

**Why platform-specific?** Each platform has different:
- Installation prefixes (`/usr/local`, `/mingw64`, `C:/vcpkg`, etc.)
- Static vs. shared library requirements (MinGW needs special handling)
- CMake flags and toolchain configurations
- System-specific workarounds (e.g., MinGW Decimal128 compatibility)

## Build System ##

Astron uses CMake as its build system, which generates build files for your platform:
- **Linux/macOS:** Unix Makefiles or Ninja
- **MSYS2 Windows:** Unix Makefiles or Ninja  
- **Visual Studio Windows:** Visual Studio solution files

### Recommended Build Tool ###

**Ninja** is recommended for all platforms as it's faster than Make and provides better output.
Install it via your package manager:
- Linux: `sudo apt install ninja-build`
- macOS: `brew install ninja`
- MSYS2: `pacman -S mingw-w64-x86_64-ninja`

## C++20 Compiler Support ##

Astron requires a C++20 compliant compiler:

| Platform         | Compiler      | Minimum Version |
|------------------|---------------|-----------------|
| Linux            | GCC           | 10+             |
| Linux            | Clang         | 11+             |
| macOS            | Apple Clang   | 12+ (Xcode 12+) |
| macOS            | LLVM Clang    | 11+             |
| Windows (MinGW)  | GCC           | 10+             |
| Windows (MSVC)   | Visual Studio | 2019+           |

## Further Information ##

For platform-specific build instructions, see:
- [Linux Build Guide](linux-gnu-make.md)
- [macOS Build Guide](macos-homebrew.md)
- [Windows MinGW Build Guide](windows-mingw.md)
- [Windows Visual Studio Build Guide](windows-visualstudio.md)

For the main build readme, see [build-readme.md](build-readme.md).

