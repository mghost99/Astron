Building with _Visual Studio_ on Windows
------------------------------------------

**Last updated: 11/11/2025**

## Table of Contents ##

1. [Requirements](#requirements)
2. [Installing Visual Studio](#installing-visual-studio)
3. [Method 1: Using vcpkg (Recommended)](#method-1-using-vcpkg-recommended)
4. [Method 2: Manual Dependency Installation](#method-2-manual-dependency-installation)
5. [Configuring CMake](#configuring-cmake)
6. [Compiling](#compiling)
7. [Running Astron](#running-astron)
8. [Troubleshooting](#troubleshooting)
9. [Quick Start Summary](#quick-start-summary-vcpkg)

---

## Requirements ##

**Astron requires C++20 support, which means you need:**
- **Visual Studio 2019 (version 16.11 or later)** - Minimum for C++20 support
- **Visual Studio 2022** - Recommended for best C++20 support

Visual Studio 2017 and earlier (including 2013, 2015) are **NOT supported** as they lack C++20 features.

### Installing Visual Studio ###

**1. Download Visual Studio:**

Choose one of these options:

**Option A: Visual Studio 2022 Community (Recommended - includes IDE):**
- Download: https://visualstudio.microsoft.com/downloads/
- Click "Free download" under "Community"
- Size: ~6 GB installed
- Completely free for individual developers, open-source projects, academic research, and small teams
- Includes the Visual Studio IDE

**Option B: Build Tools for Visual Studio 2022 (Lighter - no IDE):**
- Download: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
- Scroll down to "Tools for Visual Studio" and download "Build Tools for Visual Studio 2022"
- Size: ~3 GB installed
- No IDE, just the compiler and build tools
- Perfect if you only want to build from command line or use CMake GUI

**Option C: Visual Studio 2019 (16.11+) - Older version:**
- Download: https://visualstudio.microsoft.com/vs/older-downloads/
- Requires signing in with a free Microsoft account
- Download version 16.11 or later
- Use this if you have compatibility requirements

**2. Run the installer:**

Launch the downloaded installer (`vs_community.exe` or similar)

**3. Select workloads:**

In the Visual Studio Installer, select:
- ✅ **Desktop development with C++** (required)

This will install:
- MSVC C++ compiler
- Windows SDK
- CMake tools
- C++ core features

**Optional but recommended:**
- ✅ **C++ CMake tools for Windows** (if not included above)
- ✅ **Git for Windows** (if you don't have it)

**4. Install:**

Click **Install** and wait (this takes 10-30 minutes depending on your internet speed)

**5. Verify installation:**

After installation, open **Developer Command Prompt for VS 2022** from the Start menu and verify:

```cmd
cl
```

You should see the Microsoft C++ compiler version information.

**Note:** You don't need the full Visual Studio IDE to build Astron - the Build Tools work fine. But the Community edition is free and gives you a nice IDE if you want it.

### Prerequisites ###

Clone the repository first:

```cmd
git clone https://github.com/Astron/Astron.git
cd Astron
```

### Dependencies ###

You will need to download and build Astron's dependencies.
See the [building readme](build-readme.md) and [dependencies.md](dependencies.md) for detailed dependency information.

**Required dependencies:**
- CMake 3.15+
- Boost 1.55+
- libuv 1.23.0+
- yaml-cpp 0.5+
- OpenSSL 1.0.1+
- bison and flex (for dclass parser generation)

**There are two methods to install dependencies:**

1. **vcpkg (Recommended)** - Automated package manager for C++ libraries
2. **Manual Installation** - Download and build each dependency yourself

**Note:** If you find dependency management too complex, consider using the [MinGW-w64 guide](windows-mingw.md) instead, which uses MSYS2's package manager for much easier setup.

---

## Method 1: Using vcpkg (Recommended) ##

vcpkg is Microsoft's cross-platform package manager for C++ libraries. It makes installing dependencies much easier.

### Installing vcpkg ###

**1. Clone vcpkg:**

```cmd
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
```

**2. Bootstrap vcpkg:**

```cmd
.\bootstrap-vcpkg.bat
```

**3. Add vcpkg to your PATH (optional but recommended):**

Add `C:\vcpkg` to your system PATH environment variable.

### Installing Dependencies with vcpkg ###

Install all required Astron dependencies:

```cmd
.\vcpkg install boost:x64-windows libuv:x64-windows yaml-cpp:x64-windows openssl:x64-windows
```

For **static linking** (recommended for distribution):

```cmd
.\vcpkg install boost:x64-windows-static libuv:x64-windows-static yaml-cpp:x64-windows-static openssl:x64-windows-static
```

**Note:** Installation can take 30-60 minutes as vcpkg builds everything from source.

### Installing win_flex and win_bison ###

Astron requires flex and bison for parsing dclass files. Install the Windows versions:

```cmd
.\vcpkg install winflexbison:x64-windows
```

### Optional: PostgreSQL Support (SOCI) ###

For PostgreSQL database backend:

```cmd
.\vcpkg install soci[postgresql]:x64-windows
```

Or for static:

```cmd
.\vcpkg install soci[postgresql]:x64-windows-static
```

### vcpkg Integration with Visual Studio ###

**Option 1: System-wide integration (requires admin):**

```cmd
.\vcpkg integrate install
```

This makes all vcpkg packages automatically available to Visual Studio and CMake.

**Option 2: CMake Toolchain File (no admin required):**

When configuring CMake (see next section), you'll need to specify the vcpkg toolchain file:

In CMake GUI, add this CMake variable:
- **Name:** `CMAKE_TOOLCHAIN_FILE`
- **Type:** `FILEPATH`
- **Value:** `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`

---

## Method 2: Manual Dependency Installation ##

If you prefer not to use vcpkg, you can manually install each dependency.

### Recommended Directory Structure ###

Create a directory for all dependencies:

```
C:\Astron-deps\
├── boost\
├── libuv\
├── yaml-cpp\
├── openssl\
└── winflexbison\
```

### Installing Boost ###

**1. Download Boost:**

Download from https://www.boost.org/users/download/
- Get the latest version (1.55 or newer)
- Extract to `C:\Astron-deps\boost`

**2. Build Boost (if needed):**

Boost is mostly header-only, but you may need to build some components:

```cmd
cd C:\Astron-deps\boost
bootstrap.bat
.\b2 --build-type=complete --prefix=C:\Astron-deps\boost\install install
```

### Installing libuv ###

**1. Download libuv:**

Download from https://github.com/libuv/libuv/releases
- Get version 1.23.0 or newer
- Extract to `C:\Astron-deps\libuv`

**2. Build with CMake:**

```cmd
cd C:\Astron-deps\libuv
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=C:\Astron-deps\libuv\install
cmake --build . --config Release
cmake --install . --config Release
```

### Installing yaml-cpp ###

**1. Clone yaml-cpp:**

```cmd
cd C:\Astron-deps
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
```

**2. Build with CMake:**

```cmd
mkdir build
cd build
cmake .. -DYAML_BUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=C:\Astron-deps\yaml-cpp\install
cmake --build . --config Release
cmake --install . --config Release
```

### Installing OpenSSL ###

**1. Download pre-built OpenSSL:**

Download from https://slproweb.com/products/Win32OpenSSL.html
- Get the 64-bit version (Win64 OpenSSL)
- Install to `C:\Astron-deps\openssl`

**OR build from source** (advanced):

```cmd
cd C:\Astron-deps
git clone https://github.com/openssl/openssl.git
cd openssl
perl Configure VC-WIN64A --prefix=C:\Astron-deps\openssl\install
nmake
nmake install
```

### Installing win_flex and win_bison ###

**1. Download:**

Download from https://github.com/lexxmark/winflexbison/releases
- Extract to `C:\Astron-deps\winflexbison`

**2. Add to PATH:**

Add `C:\Astron-deps\winflexbison` to your system PATH.

### Optional: PostgreSQL (SOCI) ###

Building SOCI manually is complex. If you need PostgreSQL support, vcpkg is strongly recommended.

### Optional: MongoDB C++ Driver ###

**This is complex. Only attempt if you really need MongoDB support on Visual Studio.**

**1. (Optional) Install libmongocrypt:**

```cmd
cd C:\Astron-deps
git clone https://github.com/mongodb/libmongocrypt.git
cd libmongocrypt
mkdir build
cd build
cmake ../ -DBUILD_VERSION=1.0.0 -DCMAKE_INSTALL_PREFIX=C:\Astron-deps\libmongocrypt\install
cmake --build . --config Release
cmake --install . --config Release
cd ..\..
```

**Note:** The `-DBUILD_VERSION=1.0.0` flag is required because the build script may fail to auto-detect the version from git.

**2. Install MongoDB C Driver:**

```cmd
cd C:\Astron-deps
git clone https://github.com/mongodb/mongo-c-driver.git
cd mongo-c-driver
mkdir cmake-build
cd cmake-build
cmake .. -DCMAKE_INSTALL_PREFIX=C:\Astron-deps\mongo-c-driver\install
cmake --build . --config Release
cmake --install . --config Release
cd ..\..
```

**3. Install MongoDB C++ Driver:**

```cmd
cd C:\Astron-deps
git clone -b releases/stable https://github.com/mongodb/mongo-cxx-driver.git
cd mongo-cxx-driver\build
cmake .. -DCMAKE_INSTALL_PREFIX=C:\Astron-deps\mongo-cxx-driver\install -DCMAKE_PREFIX_PATH=C:\Astron-deps\mongo-c-driver\install
cmake --build . --config Release
cmake --install . --config Release
cd ..\..
```

---

## Configuring CMake ##

Now that dependencies are installed, you'll configure Astron with CMake.

### Using CMake GUI ###

**1. Download and install CMake:**

Download from: https://cmake.org/download/
- Get the latest version (3.15 or newer)
- During installation, choose to add CMake to PATH

**2. Launch cmake-gui**

**3. Set source and build directories:**

- **Where is the source code:** Enter the path to your Astron clone
  - Example: `C:/Users/YourName/Documents/Astron`
- **Where to build the binaries:** Use a separate build directory (recommended)
  - Example: `C:/Users/YourName/Documents/Astron-build`

**4. Click `Configure`**

**5. Select your Visual Studio version when prompted:**
- **Visual Studio 17 2022** (recommended)
- **Visual Studio 16 2019** (minimum)
- Select **x64** as the platform

### Configuration for vcpkg Users ###

**If you used vcpkg with system-wide integration (`vcpkg integrate install`):**

CMake should automatically find all dependencies. Just click `Configure` and `Generate`.

**If you're using vcpkg toolchain file (no system integration):**

After clicking `Configure` for the first time, you'll need to add the toolchain file:

1. Click **Add Entry** button
2. Add these values:
   - **Name:** `CMAKE_TOOLCHAIN_FILE`
   - **Type:** `FILEPATH`
   - **Value:** `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
3. Click **OK**
4. Click **Configure** again

CMake should now find all vcpkg-installed dependencies.

### Configuration for Manual Installation Users ###

After clicking `Configure` for the first time, CMake may not find your manually installed dependencies. You'll need to tell CMake where they are.

**Setting Dependency Paths in CMake GUI:**

For each dependency CMake can't find, you'll need to add entries. Click **Add Entry** and add the following:

#### Boost ####

If CMake can't find Boost automatically:

- **Name:** `BOOST_ROOT` or `Boost_ROOT`
- **Type:** `PATH`
- **Value:** `C:/Astron-deps/boost/install` (or wherever you installed it)

#### libuv ####

- **Name:** `LIBUV_INCLUDE_DIR`
- **Type:** `PATH`
- **Value:** `C:/Astron-deps/libuv/install/include`

- **Name:** `LIBUV_LIBRARY`
- **Type:** `FILEPATH`
- **Value:** `C:/Astron-deps/libuv/install/lib/uv.lib`

#### yaml-cpp ####

- **Name:** `YAMLCPP_INCLUDE_DIR`
- **Type:** `PATH`
- **Value:** `C:/Astron-deps/yaml-cpp/install/include`

- **Name:** `YAMLCPP_LIBRARY`
- **Type:** `FILEPATH`
- **Value:** `C:/Astron-deps/yaml-cpp/install/lib/yaml-cpp.lib`

#### OpenSSL ####

- **Name:** `OPENSSL_ROOT_DIR`
- **Type:** `PATH`
- **Value:** `C:/Astron-deps/openssl/install` (or wherever you installed it)

**After adding all necessary paths:**

1. Click **Configure** again
2. CMake should now find all dependencies
3. Check the output for any errors or missing dependencies

### Common CMake Options ###

Before generating, you can set these Astron-specific options in CMake GUI:

**Find these in the CMake variable list and modify as needed:**

- `BUILD_DBSERVER` - Build Database Server component (default: ON)
- `BUILD_DB_YAML` - Support YAML database backend (default: ON)
- `BUILD_DB_MONGO` - Support MongoDB backend (requires mongo drivers)
- `BUILD_DB_POSTGRESQL` - Support PostgreSQL backend (requires SOCI + libpq)
- `BUILD_STATESERVER` - Build State Server component (default: ON)
- `BUILD_EVENTLOGGER` - Build Event Logger component (default: ON)
- `BUILD_CLIENTAGENT` - Build Client Agent component (default: ON)
- `USE_32BIT_DATAGRAMS` - Use 32-bit length tags for datagrams (default: OFF)
- `USE_128BIT_CHANNELS` - Use 128-bit channels and 64-bit doids/zones (default: OFF)
- `BUILD_TESTS` - Compile test files (default: OFF)

### Generate the Visual Studio Solution ###

Once CMake successfully configures without errors:

1. Click **Generate** button
2. CMake will create the Visual Studio solution file (`Astron.sln`) in your build directory
3. Close cmake-gui

---

## Compiling ##

**1. Open the Visual Studio solution:**

Navigate to your build directory and double-click `Astron.sln`, or in Visual Studio:
- File → Open → Project/Solution
- Browse to your build directory and select `Astron.sln`

**2. Select build configuration:**

Use the dropdown in the Visual Studio toolbar (usually shows "Debug" by default):

- **Release** - Optimized build for production use (no debug symbols)
- **Debug** - Development build with debug symbols and trace messages (slower, larger binary)
- **RelWithDebInfo** - Optimized build with debug symbols (good for profiling)

**3. Build the solution:**

- Press **F7** or go to Build → Build Solution
- Wait for compilation to complete (first build may take several minutes)

**4. Check for errors:**

If you get build errors, check:
- All dependencies are correctly installed and found by CMake
- You're using Visual Studio 2019 16.11+ or Visual Studio 2022
- The build output window for specific error messages

### Build Artifacts ###

After successful compilation, you'll find `astrond.exe` in your build directory under:
- `Debug\astrond.exe` (for Debug builds)
- `Release\astrond.exe` (for Release builds)
- `RelWithDebInfo\astrond.exe` (for RelWithDebInfo builds)

You can rename the debug version if you want to keep both:
```cmd
move Debug\astrond.exe Debug\astrond-dbg.exe
```

### Running Astron ###

To run Astron from Command Prompt or PowerShell:

```cmd
astrond.exe --config path\to\config.yml
```

Make sure the required DLL dependencies (yaml-cpp, libuv, OpenSSL, etc.) are either in your PATH or in the same directory as `astrond.exe`.

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

Replace `test` with whatever your database is called.

---

## Troubleshooting ##

### CMake Configuration Issues ###

**"Could not find Boost"**

- Make sure Boost is installed via vcpkg or manually
- For manual installation, set `BOOST_ROOT` or `Boost_ROOT` in CMake GUI
- Try setting the environment variable `BOOST_ROOT` before running CMake

**"Could not find libuv"**

- Install via vcpkg: `.\vcpkg install libuv:x64-windows`
- For manual installation, set `LIBUV_INCLUDE_DIR` and `LIBUV_LIBRARY` in CMake GUI

**"Could not find yaml-cpp"**

- Install via vcpkg: `.\vcpkg install yaml-cpp:x64-windows`
- For manual installation, set `YAMLCPP_INCLUDE_DIR` and `YAMLCPP_LIBRARY` in CMake GUI

**"Could not find OpenSSL"**

- Install via vcpkg: `.\vcpkg install openssl:x64-windows`
- For manual installation, set `OPENSSL_ROOT_DIR` in CMake GUI
- Make sure you're using the 64-bit version of OpenSSL

**"flex/bison not found"**

- Install via vcpkg: `.\vcpkg install winflexbison:x64-windows`
- For manual installation, download from https://github.com/lexxmark/winflexbison and add to PATH

**CMake can't find vcpkg packages:**

- Make sure you ran `vcpkg integrate install` OR
- Set `CMAKE_TOOLCHAIN_FILE` to `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
- Verify packages are installed: `.\vcpkg list`

### Visual Studio Build Issues ###

**"C++20 features not available" or similar errors:**

- You need Visual Studio 2019 16.11+ or Visual Studio 2022
- Visual Studio 2017 and earlier do NOT support C++20
- Update Visual Studio to the latest version

**Link errors (LNK2001, LNK2019 - unresolved external symbols):**

- Make sure you're building the same configuration (Debug/Release) as your dependencies
- vcpkg users: ensure you installed the right triplet (x64-windows vs x64-windows-static)
- Manual users: check that all required .lib files are found

**Missing DLL errors when running:**

- For vcpkg dynamic builds, copy DLLs from `C:\vcpkg\installed\x64-windows\bin\` to your exe directory
- Or use static builds: `.\vcpkg install <package>:x64-windows-static`
- For manual builds, copy DLLs from each dependency's install directory

**Build takes very long time:**

- First-time builds always take longer
- Try building with `/MP` flag for parallel compilation (usually enabled by default)
- Consider using Ninja instead of Visual Studio generator (faster builds)

### Using Ninja with Visual Studio Compiler ###

For faster builds, you can use Ninja instead of Visual Studio's build system:

**1. Install Ninja:**

```cmd
.\vcpkg install ninja
```

Or download from: https://ninja-build.org/

**2. Configure with Ninja generator:**

In cmake-gui, delete the cache (File → Delete Cache), then:
- Click Configure
- Select **Ninja** as the generator
- Click Finish

**3. Build from command line:**

```cmd
cd <build-directory>
ninja
```

Or use Developer Command Prompt for VS and run:
```cmd
cmake --build . --config Release
```

### Alternative: MinGW-w64 vs Visual Studio ###

**When to use Visual Studio:**
- ✅ You need MongoDB support (required for VS)
- ✅ You want official Microsoft toolchain
- ✅ You're comfortable with larger downloads
- ✅ vcpkg makes dependency management automatic

**When to use MinGW-w64:**
- ✅ You only need YAML or PostgreSQL databases (no MongoDB)
- ✅ You want faster setup with smaller downloads
- ✅ You prefer Unix-like build tools
- ✅ MSYS2's package manager is simpler for non-MongoDB dependencies

See the [MinGW-w64 guide](windows-mingw.md) for the alternative approach.

---

## Quick Start Summary (vcpkg) ##

Here's a complete workflow using vcpkg (recommended for most users):

```cmd
# 0. Install Visual Studio 2022 Community
# - Download from https://visualstudio.microsoft.com/downloads/
# - Run installer and select "Desktop development with C++" workload
# - Wait for installation (10-30 minutes)

# 1. Install vcpkg
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 2. Install dependencies (30-60 minutes)
.\vcpkg install boost:x64-windows libuv:x64-windows yaml-cpp:x64-windows openssl:x64-windows winflexbison:x64-windows

# 2b. OPTIONAL: Add MongoDB support (adds 1-2 hours)
.\vcpkg install mongo-cxx-driver:x64-windows

# 3. Integrate with Visual Studio
.\vcpkg integrate install

# 4. Clone Astron
cd C:\
git clone https://github.com/Astron/Astron.git

# 5. Configure with CMake GUI
# - Launch cmake-gui
# - Set source: C:/Astron
# - Set build: C:/Astron-build
# - Click Configure (select VS 2022 x64)
# - Click Generate

# 6. Build in Visual Studio
# - Open C:\Astron-build\Astron.sln
# - Select Release configuration
# - Press F7 to build

# 7. Run
cd C:\Astron-build\Release
.\astrond.exe --config path\to\config.yml
```

**Total time estimate:** 2-3 hours (mostly waiting for installations)

---

## Further Information ##

For dependency details and general build information, see the main [build readme](build-readme.md) and [dependencies.md](dependencies.md).

For running and configuring Astron, refer to the main [README](../../README.md).
