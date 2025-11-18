Building with _MinGW-w64_ on Windows
--------------------------------------

**Last updated: 11/12/2025**
*This guide uses MSYS2 with MinGW-w64 (no Visual Studio required)*

### Overview ###

This guide explains how to build Astron on Windows without Visual Studio, using MSYS2 and MinGW-w64.
MSYS2 provides a Unix-like environment on Windows with a package manager (pacman) that makes installing dependencies straightforward.

### Prerequisites ###

You will need to clone the repository first:

```bash
git clone https://github.com/Astron/Astron.git
cd Astron
```

### Installing MSYS2 ###

1. Download the latest MSYS2 installer from https://www.msys2.org/
2. Run the installer as Administrator (right-click → "Run as administrator")
3. Follow the installation wizard (default location: `C:\msys64`)
4. After installation, launch **MSYS2 MINGW64** (not MSYS2 MSYS or UCRT64)

**Note:** If the installer appears stuck, cancel it and try running as Administrator or temporarily disable antivirus software.

**Note:** Always use the **MINGW64** environment for building Astron to ensure compatibility.

### Update MSYS2 ###

Before installing packages, update MSYS2:

```bash
pacman -Syu
```

When the terminal closes, reopen **MSYS2 MINGW64** and refresh keys, then run the update again:

```bash
pacman-key --keyserver keyserver.ubuntu.com --refresh-keys
pacman -Su
```

### Dependencies ###

#### Required Dependencies ####

Install all required build tools and dependencies.
When prompted how you would like to continue, hit enter for the `mingw-w64-x86_64-toolchain` package `(default=all)`.

```bash
pacman -S --needed base-devel mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-boost \
    mingw-w64-x86_64-libuv \
    mingw-w64-x86_64-yaml-cpp \
    mingw-w64-x86_64-openssl \
    git
```



This will install:
- **base-devel**: Essential build tools (make, autoconf, etc.)
- **mingw-w64-x86_64-toolchain**: GCC compiler, g++, and related tools
- **mingw-w64-x86_64-cmake**: CMake build system (version 3.15+)
- **mingw-w64-x86_64-ninja**: Ninja build tool (faster alternative to Make)
- **mingw-w64-x86_64-boost**: Boost C++ libraries (version 1.55+)
- **mingw-w64-x86_64-libuv**: Cross-platform asynchronous I/O library (version 1.23.0+)
- **mingw-w64-x86_64-yaml-cpp**: YAML parser library (version 0.5+)
- **mingw-w64-x86_64-openssl**: SSL/TLS cryptography library (version 1.0.1+)
- **git**: Version control system

#### Optional: Database Backend Dependencies ####

There is a built-in YAML database but if you want a fully featured database backend, you're in the right spot!


**MongoDB Support:**

MongoDB C++ drivers require disabling Decimal128 support due to compiler incompatibilities with Intel's DFP library. This is fine for most use cases as Decimal128 is only needed for extreme financial precision calculations.

**To build MongoDB support on MinGW:**

```bash
# Install MongoDB build dependencies
pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-icu mingw-w64-x86_64-pcre \
    mingw-w64-x86_64-snappy mingw-w64-x86_64-zlib mingw-w64-x86_64-zstd \
    mingw-w64-x86_64-cyrus-sasl
```

**(Optional) Build and install libmongocrypt:**

```bash
git clone https://github.com/mongodb/libmongocrypt
cd libmongocrypt
mkdir build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/mingw64
-DBUILD_VERSION=1.17.0 
-DMONGOCRYPT_ENABLE_DECIMAL128=OFF
ninja  # or 'make' if you don't have ninja installed
ninja install  # or 'make install'
cd ../..
```

**Note:** The `-DBUILD_VERSION=1.17.0` flag is required because the build script may fail to auto-detect the version from git. If you have Ninja installed (which you should from earlier steps), CMake will automatically use it instead of Make.

**Install mongo-c-driver:**

```bash
wget https://github.com/mongodb/mongo-c-driver/releases/download/2.1.2/mongo-c-driver-2.1.2.tar.gz
tar xzf mongo-c-driver-2.1.2.tar.gz
cd mongo-c-driver-2.1.2
mkdir cmake-build
cd cmake-build

cmake .. \
    -DCMAKE_PREFIX_PATH=/mingw64 \
    -DENABLE_CLIENT_SIDE_ENCRYPTION=ON \
    -DCMAKE_INSTALL_PREFIX=/mingw64 \
    -DCMAKE_BUILD_TYPE=Release

ninja
ninja install
cd ../..
```

**Note:** `CMAKE_PREFIX_PATH=/mingw64` tells mongo-c-driver where to find libmongocrypt.

**Install mongo-cxx-driver:**

```bash
git clone -b releases/stable https://github.com/mongodb/mongo-cxx-driver.git
cd mongo-cxx-driver/build

cmake .. \
    -DBUILD_VERSION=4.1.4 \
    -DCMAKE_PREFIX_PATH=/mingw64 \
    -DCMAKE_INSTALL_PREFIX=/mingw64 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_SHARED_AND_STATIC_LIBS=OFF

ninja
ninja install
cd ../..
```

### Preparing the Build ###

Navigate to your Astron directory. In MSYS2, Windows drives are accessible under `/c/`, `/d/`, etc.

For example, if you cloned Astron to `C:\Users\YourName\Documents\Astron`:

```bash
cd /c/Users/YourName/Documents/Astron
```

CD into the `build/` directory:

```bash
cd build
```

### Compiling ###

After setting up your environment, you can compile Astron.

**Navigate to the build directory:**

cd into the `Astron` folder and then cd into the `build` or `build-debug` folder.

#### Option 1: Using Ninja (Recommended - Faster) ####

**For release:**

```bash
cd Astron/build

# Configure with full database support (YAML + MongoDB)
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DLIBMONGOCXX_INCLUDE_DIRS=/mingw64/include/mongocxx/v_noabi \
    -DLIBMONGOCXX_LIBRARIES=/mingw64/lib/libmongocxx-static.a \
    -DLIBBSONCXX_INCLUDE_DIRS=/mingw64/include/bsoncxx/v_noabi \
    -DLIBBSONCXX_LIBRARIES=/mingw64/lib/libbsoncxx-static.a \
    ..

ninja
```

**Note:** MongoDB paths must be specified manually on MinGW because:
- mongo-cxx-driver static libraries use `-static` suffix in filenames

**For development (with Trace and Debug messages):**

```bash
cd Astron/build-debug
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

**For development (without Trace and Debug messages):**

```bash
cd Astron/build
cmake -G "Ninja" ..
ninja
```

#### Option 2: Using Make ####

**For release:**

```bash
cd Astron/build

# Configure with full database support (YAML + PostgreSQL + MySQL + SQLite3 + MongoDB)
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DSOCI_INCLUDE_DIR=/mingw64/include \
    -DSOCI_LIBRARY=/mingw64/lib/libsoci_core_4_1.dll.a \
    -DSOCI_LIBRARY_DIR=/mingw64/lib \
    -DSOCI_postgresql_PLUGIN=/mingw64/lib/libsoci_postgresql_4_1.dll.a \
    -DSOCI_mysql_PLUGIN=/mingw64/lib/libsoci_mysql_4_1.dll.a \
    -DSOCI_sqlite3_PLUGIN=/mingw64/lib/libsoci_sqlite3_4_1.dll.a \
    -DLIBMONGOCXX_INCLUDE_DIRS=/mingw64/include/mongocxx/v_noabi \
    -DLIBMONGOCXX_LIBRARIES=/mingw64/lib/libmongocxx-static.a \
    -DLIBBSONCXX_INCLUDE_DIRS=/mingw64/include/bsoncxx/v_noabi \
    -DLIBBSONCXX_LIBRARIES=/mingw64/lib/libbsoncxx-static.a \
    ..

make
```

**For development (with Trace and Debug messages):**

```bash
cd Astron/build-debug
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
make
```

**For development (without Trace and Debug messages):**

```bash
cd Astron/build
cmake -G "Unix Makefiles" ..
make
```

**Note:** You can use parallel builds to speed up compilation:
- For Ninja: `ninja -j<N>` (e.g., `ninja -j4` for 4 parallel jobs)
- For Make: `make -j<N>` (e.g., `make -j4` for 4 parallel jobs)

Ninja automatically uses parallel builds by default.

### Build Artifacts ###

You should now have `astrond.exe` in your builds folder.

Both release and debug use the same name `astrond.exe`, so if you build debug first, we can just rename it!

```bash
mv astrond.exe astrond-dbg.exe
```

Then you can build the release version.

To run Astron, you'll need to ensure the MinGW DLLs are in your PATH or copy them alongside `astrond.exe`.

### Running Astron ###

#### Option 1: Run from MSYS2 MINGW64 terminal ####

The MSYS2 MINGW64 environment automatically includes the necessary DLLs in PATH:

```bash
./astrond.exe --config path/to/config.yml
```

#### Option 2: Standalone execution ####

To run `astrond.exe` outside MSYS2, copy the required DLLs from `C:\msys64\mingw64\bin\` to the same directory as `astrond.exe`:

- `libyaml-cpp.dll`
- `libuv-1.dll`
- `libssl-*.dll`
- `libcrypto-*.dll`
- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`

You can use the `ldd` command in MSYS2 to see all required DLLs:

```bash
ldd astrond.exe
```

Then copy all non-system DLLs (those from `/mingw64/bin/`) to your astrond directory.

### Configuration ###

For those who currently use the YAML database and want to use MongoDB instead,
replace the following in your config file:

```yaml
backend:
  type: yaml
  directory: ../databases/astrondb
```

with:

```yaml
backend:
  type: mongodb
  server: mongodb://127.0.0.1/test
```

Replace `test` with whatever your database is called. This will allow remote MongoDB instances for those wondering (or rather it should unless I borked something..)

### Common Build Options ###

Astron supports several CMake options. Here's a full example with **all database backends** enabled:

```bash
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DSOCI_INCLUDE_DIR=/mingw64/include/soci \
    -DSOCI_INCLUDE_DIRS=/mingw64/include/soci \
    -DSOCI_LIBRARY=/mingw64/lib/libsoci_core_4_1.dll.a \
    -DSOCI_LIBRARY_DIR=/mingw64/lib \
    -DSOCI_postgresql_PLUGIN=/mingw64/lib/libsoci_postgresql_4_1.dll.a \
    -DSOCI_mysql_PLUGIN=/mingw64/lib/libsoci_mysql_4_1.dll.a \
    -DSOCI_sqlite3_PLUGIN=/mingw64/lib/libsoci_sqlite3_4_1.dll.a \
    -DLIBMONGOCXX_INCLUDE_DIRS=/mingw64/include/mongocxx/v_noabi \
    -DLIBMONGOCXX_LIBRARIES="C:/msys64/mingw64/lib/libmongocxx-static.a;C:/msys64/mingw64/lib/libmongoc2.a;C:/msys64/mingw64/lib/libmongocrypt.dll.a" \
    -DLIBBSONCXX_INCLUDE_DIRS=/mingw64/include/bsoncxx/v_noabi \
    -DLIBBSONCXX_LIBRARIES="C:/msys64/mingw64/lib/libbsoncxx-static.a;C:/msys64/mingw64/lib/libbson2.a" \
    ..

ninja
```

**Note:** Use Unix-style paths (`/mingw64/...`) for include directories, but Windows-style paths (`C:/msys64/mingw64/...`) for library files in semicolon-separated lists.

**System libraries (automatically included):** The CMakeLists.txt automatically adds Windows system libraries needed by MongoDB: `dnsapi`, `bcrypt`, `ncrypt`, `secur32`, `crypt32`, `snappy`, `zstd`. You don't need to specify these manually.

**For YAML-only (no MongoDB):**

```bash
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_DB_MONGO=OFF \
    ..

ninja
```

**Available Options:**
- `USE_32BIT_DATAGRAMS`: Use 32-bit length tags for datagrams (default: OFF)
- `BUILD_TESTS`: Compile test files (default: OFF)
- `BUILD_DBSERVER`: Build Database Server component (default: ON)
- `BUILD_DB_YAML`: Support YAML database backend (default: ON)
- `BUILD_DB_MONGO`: Support MongoDB backend (requires mongo drivers)
- `BUILD_STATESERVER`: Build State Server component (default: ON)
- `BUILD_EVENTLOGGER`: Build Event Logger component (default: ON)
- `BUILD_CLIENTAGENT`: Build Client Agent component (default: ON)

### Troubleshooting ###

#### CMake can't find dependencies ####

Make sure you're running from the **MSYS2 MINGW64** terminal (not MSYS or UCRT64).
The MinGW packages are only available in the MINGW64 environment.

#### CMake can't find MongoDB client library ####

**This is also expected!** Static MongoDB libraries don't create pkg-config files, so you must specify paths manually:

**Verify MongoDB is installed:**

```bash
ls /mingw64/lib/libmongocxx* /mingw64/lib/libbsoncxx* /mingw64/lib/libmongoc* /mingw64/lib/libbson*
# Should show: libmongocxx-static.a, libbsoncxx-static.a, libmongoc2.a, libbson2.a, libmongocrypt.dll.a

ls /mingw64/include/mongocxx/v_noabi/ /mingw64/include/bsoncxx/v_noabi/
# Should show header files
```

**The MongoDB paths are already included in the build commands above.** 

**Important:** The full MongoDB dependency chain is:
- C++ driver: `libmongocxx-static.a` + `libbsoncxx-static.a`
- C driver: `libmongoc2.a` + `libbson2.a`  
- Encryption: `libmongocrypt.dll.a` (DLL version, not `-static`)

**Note:** We use the DLL version of libmongocrypt (`libmongocrypt.dll.a`) because the static version has DLL exports and won't link properly with MinGW. Similarly, yaml-cpp uses the DLL version automatically.

#### Building without database backends ####

**If you only want YAML (no SQL or MongoDB):**

```bash
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_DB_POSTGRESQL=OFF \
    -DBUILD_DB_MYSQL=OFF \
    -DBUILD_DB_SQLITE=OFF \
    -DBUILD_DB_MONGO=OFF \
    ..
```

#### Compiler not found ####

Ensure `mingw-w64-x86_64-toolchain` is installed:

```bash
pacman -S mingw-w64-x86_64-toolchain
```

Verify GCC is available:

```bash
gcc --version
g++ --version
```

#### Boost headers not found ####

Reinstall Boost:

```bash
pacman -S mingw-w64-x86_64-boost
```

#### Link errors with undefined references ####

This usually means a library is missing. Check that all dependencies are installed:

```bash
pacman -Qs mingw-w64-x86_64-yaml-cpp
pacman -Qs mingw-w64-x86_64-libuv
pacman -Qs mingw-w64-x86_64-openssl
pacman -Qs mingw-w64-x86_64-boost
```

#### Runtime DLL errors ####

When running `astrond.exe` outside MSYS2, you need to copy the required DLLs.
Use `ldd astrond.exe` in MSYS2 to identify all dependencies.

### Alternative: Using Windows Command Prompt or PowerShell ###

While you need MSYS2 to build, you can add the MinGW bin directory to your Windows PATH to run from cmd or PowerShell:

1. Add `C:\msys64\mingw64\bin` to your Windows PATH environment variable
2. Open a new Command Prompt or PowerShell
3. Navigate to your build directory
4. Run: `astrond.exe --config path\to\config.yml`

**Note:** Be careful with PATH pollution. This may cause conflicts with other tools.

### Clean Build ###

If you need to start fresh:

```bash
# From build directory
ninja clean  # or: make clean

# Or delete and recreate the build directory
cd ..
rm -rf build
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### Further Information ###

For dependency details and general build information, see the main [build readme](build-readme.md).

For running and configuring Astron, refer to the main [README](../../README.md).
