How to Build Astron
====================

## So you want to build Astron? Let's get started! ##

Astron uses **CMake** as its cross-platform build system. Choose the guide for your platform below:

### Build Guides by Platform ###

📋 **Linux**  
→ [Linux Build Guide (Make)](linux-gnu-make.md)

🍎 **macOS**  
→ [macOS Build Guide (Homebrew)](macos-homebrew.md)

🪟 **Windows**  
→ [Windows MinGW Build Guide](windows-mingw.md) - **Recommended** (no Visual Studio needed)  
→ [Windows Visual Studio Build Guide](windows-visualstudio.md) - Alternative approach

### What You'll Need ###

All platforms require these minimum dependency versions:

| Dependency   | Version  |
|--------------|----------|
| CMake        | 3.15+    |
| C++ Compiler | C++20    |
| Boost        | 1.55+    |
| libuv        | 1.23.0+  |
| yaml-cpp     | 0.5+     |
| OpenSSL      | 1.0.1+   |
| Bison & Flex | Any      |

**For detailed dependency information:** See [dependencies.md](dependencies.md)

### Optional Database Backends ###

Astron includes a built-in **YAML database** (perfect for development). You can optionally add:

- **MongoDB** (requires manual driver builds)

Each platform guide includes instructions for installing optional database support.

### Quick Links ###

- **Getting a copy of the code:**
  ```bash
  git clone https://github.com/Astron/Astron.git
  cd Astron
  ```

- **Dependencies reference:** [dependencies.md](dependencies.md)
- **Configuration docs:** [../config/](../config/)
- **Main README:** [../../README.md](../../README.md)

---

**Ready to build?** Pick your platform above and follow the step-by-step guide!
