Building with _Homebrew_ on macOS
-----------------------------------

**Last updated: 11/09/2025**
*This guide uses Homebrew package manager for macOS*

### Overview ###

This guide explains how to build Astron on macOS using Homebrew.
Homebrew provides an easy way to install all required dependencies and build tools.

### Prerequisites ###

You will need to clone the repository first:

```bash
git clone https://github.com/Astron/Astron.git
cd Astron
```

### Installing Homebrew ###

If you don't have Homebrew installed, install it first:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

After installation, follow the instructions to add Homebrew to your PATH.

### Installing Xcode Command Line Tools ###

Ensure you have Xcode Command Line Tools installed:

```bash
xcode-select --install
```

### Dependencies ###

#### Required Dependencies ####

Install all required build tools and dependencies:

```bash
brew install cmake ninja boost libuv yaml-cpp openssl git bison flex
```

This will install:
- **cmake**: CMake build system (version 3.15+)
- **ninja**: Ninja build tool (faster alternative to Make)
- **boost**: Boost C++ libraries (version 1.55+)
- **libuv**: Cross-platform asynchronous I/O library (version 1.23.0+)
- **yaml-cpp**: YAML parser library (version 0.5+)
- **openssl**: SSL/TLS cryptography library (version 1.0.1+)
- **git**: Version control system
- **bison**: Parser generator
- **flex**: Fast lexical analyzer

### Optional: MongoDB Support ###

If you want MongoDB database backend support, install additional dependencies:

```bash
brew install mongodb-community curl icu4c pcre snappy zlib zstd
```

#### (Optional) Build and install libmongocrypt ####

```bash
git clone https://github.com/mongodb/libmongocrypt
cd libmongocrypt
mkdir cmake-build && cd cmake-build
cmake ../ -DBUILD_VERSION=1.0.0
make
sudo make install
cd ../..
```

**Note:** The `-DBUILD_VERSION=1.0.0` flag is required because the build script may fail to auto-detect the version from git.

#### Install mongo-c-driver ####

```bash
curl -LO https://github.com/mongodb/mongo-c-driver/releases/download/2.1.2/mongo-c-driver-2.1.2.tar.gz
tar xzf mongo-c-driver-2.1.2.tar.gz
cd mongo-c-driver-2.1.2
mkdir cmake-build
cd cmake-build

cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_BUILD_TYPE=Release

make
sudo make install
cd ../..
```

#### Install mongo-cxx-driver ####

```bash
git clone -b releases/stable https://github.com/mongodb/mongo-cxx-driver.git
cd mongo-cxx-driver/build

cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_INSTALL_RPATH=/usr/local/lib \
    -DCMAKE_BUILD_TYPE=Release

sudo make 
sudo make install
cd ../..
```

### Optional: PostgreSQL Support ###

For PostgreSQL database backend (using SOCI):

```bash
brew install postgresql soci
```

### Compiling ###

After setting up your environment, you can compile Astron.

**Navigate to the build directory:**

cd into the `Astron` folder and then cd into the `build` or `build-debug` folder.

#### Option 1: Using Ninja (Recommended - Faster) ####

**For release:**

```bash
cd Astron/build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

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
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

**For development (with Trace and Debug messages):**

```bash
cd Astron/build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

**For development (without Trace and Debug messages):**

```bash
cd Astron/build
cmake ..
make
```

**Note:** You can use parallel builds to speed up compilation:
- For Ninja: `ninja -j<N>` (e.g., `ninja -j4` for 4 parallel jobs)
- For Make: `make -j<N>` (e.g., `make -j4` for 4 parallel jobs)

Ninja automatically uses parallel builds by default.

### Build Artifacts ###

You should now have `astrond` in your builds folder.

Both release and debug use the same name `astrond`, so if you build debug first, we can just rename it!

```bash
mv astrond astrond-dbg
```

Then you can build the release version.

Now let's make it executable:

```bash
chmod +x astrond
```

Copy it wherever you need it, and we're basically done!

### Running Astron ###

To run Astron:

```bash
./astrond --config path/to/config.yml
```

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

Astron supports several CMake options that can be set during configuration:

```bash
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DUSE_32BIT_DATAGRAMS=OFF \
    -DUSE_128BIT_CHANNELS=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_DBSERVER=ON \
    -DBUILD_DB_YAML=ON \
    -DBUILD_STATESERVER=ON \
    -DBUILD_EVENTLOGGER=ON \
    -DBUILD_CLIENTAGENT=ON \
    ..
```

**Available Options:**
- `USE_32BIT_DATAGRAMS`: Use 32-bit length tags for datagrams (default: OFF)
- `USE_128BIT_CHANNELS`: Use 128-bit channels and 64-bit doids/zones (default: OFF)
- `BUILD_TESTS`: Compile test files (default: OFF)
- `BUILD_DBSERVER`: Build Database Server component (default: ON)
- `BUILD_DB_YAML`: Support YAML database backend (default: ON)
- `BUILD_DB_MONGO`: Support MongoDB backend (requires mongo drivers)
- `BUILD_DB_POSTGRESQL`: Support PostgreSQL backend (requires SOCI + libpq)
- `BUILD_STATESERVER`: Build State Server component (default: ON)
- `BUILD_EVENTLOGGER`: Build Event Logger component (default: ON)
- `BUILD_CLIENTAGENT`: Build Client Agent component (default: ON)

### Troubleshooting ###

#### CMake can't find OpenSSL ####

Homebrew installs OpenSSL in a non-standard location. You may need to tell CMake where to find it:

```bash
export OPENSSL_ROOT_DIR=$(brew --prefix openssl)
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
```

Or specify it directly in the CMake command:

```bash
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) \
    ..
```

#### CMake can't find Boost ####

If CMake has trouble finding Boost, you can specify the Boost root:

```bash
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DBOOST_ROOT=$(brew --prefix boost) \
    ..
```

#### Compiler not found or wrong version ####

Make sure you have Xcode Command Line Tools installed:

```bash
xcode-select --install
```

Verify your compiler:

```bash
clang --version
```

#### Link errors with undefined references ####

This usually means a library is missing. Check that all dependencies are installed:

```bash
brew list | grep -E "yaml-cpp|libuv|openssl|boost"
```

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

