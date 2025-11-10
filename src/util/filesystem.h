#pragma once
#include <filesystem>
#include <string>

// Modern C++17 filesystem wrapper for Astron
// Provides compatibility layer with old fs namespace
namespace fs
{ 
    namespace stdfs = std::filesystem;

    [[nodiscard]] inline bool file_exists(const std::string& path)
    {
        std::error_code ec;
        return stdfs::is_regular_file(path, ec);
    }

    [[nodiscard]] inline bool dir_exists(const std::string& path)
    {
        std::error_code ec;
        return stdfs::is_directory(path, ec);
    }

    [[nodiscard]] inline bool is_readable(const std::string& path)
    {
        std::error_code ec;
        auto perms = stdfs::status(path, ec).permissions();
        if(ec) {
            return false;
        }
        using stdfs::perms;
        return (perms & perms::owner_read) != perms::none ||
               (perms & perms::group_read) != perms::none ||
               (perms & perms::others_read) != perms::none;
    }

    [[nodiscard]] inline std::string parent_of(const std::string& path)
    {
        if(path.empty()) {
            return path;
        }
        
        stdfs::path p(path);
        return p.parent_path().string();
    }

    [[nodiscard]] inline std::string filename(const std::string& path)
    {
        if(path.empty()) {
            return path;
        }
        
        stdfs::path p(path);
        return p.filename().string();
    }

    inline bool current_path(const std::string& path)
    {
        std::error_code ec;
        if(!dir_exists(path)) {
            return false;
        }
        
        stdfs::current_path(path, ec);
        return !ec;
    }

    [[nodiscard]] inline std::string current_path()
    {
        std::error_code ec;
        auto p = stdfs::current_path(ec);
        if(ec) {
            return "";
        }
        return p.string();
    }

};
