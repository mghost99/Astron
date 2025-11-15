#include "config/ConfigSection.h"

#include <sstream>

ConfigSection::ConfigSection() : m_node(), m_path("$")
{
}

ConfigSection::ConfigSection(const YAML::Node &node, std::string path) :
    m_node(node),
    m_path(std::move(path))
{
}

bool ConfigSection::defined() const
{
    return m_node && m_node.IsDefined();
}

const std::string &ConfigSection::path() const
{
    return m_path;
}

bool ConfigSection::has(const std::string &key) const
{
    return resolve(key).IsDefined();
}

ConfigSection ConfigSection::child(const std::string &key) const
{
    YAML::Node value = resolve(key);
    return ConfigSection(value, key_path(key));
}

std::vector<ConfigSection> ConfigSection::sequence(const std::string &key) const
{
    std::vector<ConfigSection> out;
    YAML::Node seq = resolve(key);
    if(!seq) {
        return out;
    }
    if(!seq.IsSequence()) {
        throw std::runtime_error(key_path(key) + " must be a list");
    }
    for(std::size_t i = 0; i < seq.size(); ++i) {
        std::stringstream p;
        p << key_path(key) << "[" << i << "]";
        out.emplace_back(seq[i], p.str());
    }
    return out;
}

std::vector<std::string> ConfigSection::keys() const
{
    std::vector<std::string> out;
    if(!m_node || !m_node.IsMap()) {
        return out;
    }
    for(auto it = m_node.begin(); it != m_node.end(); ++it) {
        out.emplace_back(it->first.as<std::string>());
    }
    return out;
}

const YAML::Node &ConfigSection::yaml() const
{
    return m_node;
}

YAML::Node ConfigSection::resolve(const std::string &key) const
{
    if(!m_node) {
        return YAML::Node();
    }
    YAML::Node child = m_node[key];
    return child;
}

std::string ConfigSection::key_path(const std::string &key) const
{
    if(m_path == "$") {
        return "$." + key;
    }
    return m_path + "." + key;
}

