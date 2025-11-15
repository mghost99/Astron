#include <sstream>
#pragma once

#include <yaml-cpp/yaml.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class ConfigSection
{
  public:
    ConfigSection();
    ConfigSection(const YAML::Node &node, std::string path);

    bool defined() const;
    const std::string &path() const;

    bool has(const std::string &key) const;

    ConfigSection child(const std::string &key) const;
    std::vector<ConfigSection> sequence(const std::string &key) const;

    template<typename T>
    std::optional<T> get_optional(const std::string &key) const;

    template<typename T>
    T get_or(const std::string &key, const T &fallback) const;

    template<typename T>
    T require(const std::string &key) const;

    template<typename T>
    std::optional<T> get_optional_as(const std::string &key) const;

    std::vector<std::string> keys() const;

    const YAML::Node &yaml() const;

  private:
    YAML::Node m_node;
    std::string m_path;

    YAML::Node resolve(const std::string &key) const;
    std::string key_path(const std::string &key) const;
};

template<typename T>
std::optional<T> ConfigSection::get_optional_as(const std::string &key) const
{
    YAML::Node value = resolve(key);
    if(!value || !value.IsDefined()) {
        return std::nullopt;
    }
    try {
        return value.as<T>();
    } catch(const std::exception &e) {
        throw std::runtime_error(key_path(key) + ": " + e.what());
    }
}

template<typename T>
std::optional<T> ConfigSection::get_optional(const std::string &key) const
{
    return get_optional_as<T>(key);
}

template<typename T>
T ConfigSection::get_or(const std::string &key, const T &fallback) const
{
    auto value = get_optional<T>(key);
    if(value) {
        return *value;
    }
    return fallback;
}

template<typename T>
T ConfigSection::require(const std::string &key) const
{
    auto value = get_optional<T>(key);
    if(!value) {
        throw std::runtime_error(key_path(key) + " is required");
    }
    return *value;
}

