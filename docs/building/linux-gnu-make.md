Building with GNU _Make_ on Linux
-----------------------------------

**Last updated: 11/09/2025**
*The commands below assume you are using a Linux distro with a base of 25.10 (Ubuntu/Debian)*

### Dependencies ###

You will need to clone the repository first.
After you have the repository, you will need to download and build Astron's dependencies.

#### Required Dependencies ####

Install all packages needed for Astron itself:

```bash
sudo apt install bison curl flex g++ git gnupg libboost-dev libicu-dev libpq-dev libsasl2-dev libssl-dev libuv1-dev libyaml-cpp-dev libzstd-dev pkg-config 
```

This will install:
- **bison**: Parser generator
- **curl**: HTTP client library
- **flex**: Fast lexical analyzer
- **g++**: GNU C++ compiler
- **git**: Version control system
- **gnupg**: GNU Privacy Guard
- **libboost-dev**: Boost C++ libraries (version 1.55+)
- **libicu-dev**: International Components for Unicode
- **libpq-dev**: PostgreSQL client library
- **libsasl2-dev**: SASL authentication library
- **libssl-dev**: OpenSSL development files (version 1.0.1+)
- **libuv1-dev**: Cross-platform asynchronous I/O library (version 1.23.0+)
- **libyaml-cpp-dev**: YAML parser library (version 0.5+)
- **libzstd-dev**: Zstandard compression library
- **pkg-config**: Package configuration tool

### Optional: MongoDB Support ###

If you want MongoDB database backend support, install additional dependencies:

```bash
sudo apt install -y git build-essential pkg-config libcurl4-openssl-dev libssl-dev libpcap-dev libpcre3-dev libsnappy-dev zlib1g-dev libgoogle-perftools-dev python3 python3-pip python3-venv python-pymongo libreadline-dev libstemmer-dev
```

#### Installing MongoDB Compass ####

Install MongoDB Compass (GUI tool):

```bash
sudo apt-get install -y mongodb-compass
```

#### Installing MongoDB Community Server ####

```bash
wget https://repo.mongodb.org/apt/ubuntu/dists/noble/mongodb-org/8.2/multiverse/binary-amd64/mongodb-org-server_8.2.1_amd64.deb
sudo dpkg -i mongodb-org-server_8.2.1_amd64.deb
```

#### (Optional) Build and install libmongocrypt ####

```bash
git clone https://github.com/mongodb/libmongocrypt
cd libmongocrypt
mkdir cmake-build && cd cmake-build
cmake ../ -DBUILD_VERSION=1.0.0
make
sudo make install
```

**Note:** The `-DBUILD_VERSION=1.0.0` flag is required because the build script may fail to auto-detect the version from git.

#### Install mongo-c-driver ####

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

After setting up your environment, you can compile Astron.

**Clone the repository** (if you haven't already):

```bash
git clone https://github.com/Astron/Astron.git
```

**Navigate to the build directory:**

cd into the `Astron` folder and then cd into the `build` or `build-debug` folder.

#### For release: ####

```bash
cd Astron/build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

#### For development (with Trace and Debug messages): ####

```bash
cd Astron/build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

#### For development (without Trace and Debug messages): ####

```bash
cd Astron/build
cmake ..
make
```

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

### Further Information ###

For more details about dependencies and general build information, see the main [build readme](build-readme.md).

For running and configuring Astron, refer to the main [README](../../README.md).
