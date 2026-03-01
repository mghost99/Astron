# Building with GNU _Make_ on Arch Linux
-----------------------------------

**Last updated: 03/01/2026**
*The commands below assume you are using Arch Linux or an Arch-based distro.*

### Dependencies ###

You will need to clone the repository first.
After you have the repository, you will need to download and build Astron's dependencies.

#### Required Dependencies ####

Install all packages needed for Astron itself:

```bash
sudo pacman -S --needed base-devel cmake boost boost-libs icu libsasl openssl libuv yaml-cpp zstd postgresql-libs curl flex bison git gnupg pkgconf mongo-c-driver
```

This will install:
- **base-devel**: Common build tools (gcc, make, etc.)
- **cmake**: Build system generator
- **boost / boost-libs**: Boost C++ libraries
- **icu**: International Components for Unicode
- **libsasl**: SASL authentication library
- **openssl**: OpenSSL development files
- **libuv**: Cross-platform asynchronous I/O library
- **yaml-cpp**: YAML parser library
- **zstd**: Zstandard compression library
- **postgresql-libs**: PostgreSQL client library (for libpq)
- **curl**: HTTP client library
- **flex**: Fast lexical analyzer
- **bison**: Parser generator
- **git**: Version control system
- **gnupg**: GNU Privacy Guard
- **pkgconf**: Package configuration tool
- **mongo-c-driver**: C client library for MongoDB (required for CXX driver build)

### Optional: MongoDB Support ###

If you want MongoDB database backend support, it's recommended to install the drivers from the AUR (Arch User Repository) using a helper like `yay`.

#### Installing MongoDB Drivers (Recommended) ####

```bash
yay -S mongo-cxx-driver
```

This will automatically handle `mongo-c-driver` and `libmongocrypt` dependencies.

#### Alternative: Manual Installation ####

If you prefer building from source manually, follow these steps.

#### Install mongo-c-driver ####

While `mongo-c-driver` is available in `extra`, building from source ensures matching versions if needed:

```bash
wget https://github.com/mongodb/mongo-c-driver/releases/download/2.1.2/mongo-c-driver-2.1.2.tar.gz
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

### Compiling ###

After setting up your environment, you can compile Astron. It is recommended to use modern CMake commands to avoid directory confusion.

**Clone the repository** (if you haven't already):

```bash
git clone https://github.com/Astron/Astron.git
cd Astron
```

#### For release: ####

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

#### For development (with Trace and Debug messages): ####

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc)
```

#### For development (without Trace and Debug messages): ####

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

### Build Artifacts ###

You should now have `astrond` in your builds folder.

```bash
chmod +x astrond
```

### Further Information ###

For more details about dependencies and general build information, see the main [build readme](build-readme.md).

For running and configuring Astron, refer to the main [README](../../README.md).
