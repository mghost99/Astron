#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class ConfigSchema;

namespace config {

enum class SchemaValueType {
    String,
    Bool,
    Int,
    UInt,
    Number,
    Object
};

struct SchemaNode;

struct SchemaProperty {
    SchemaProperty();
    SchemaProperty(const SchemaProperty &other);
    SchemaProperty& operator=(const SchemaProperty &other);
    SchemaProperty(SchemaProperty &&) noexcept = default;
    SchemaProperty& operator=(SchemaProperty &&) noexcept = default;

    SchemaValueType type;
    bool required;
    std::vector<std::string> enum_values;
    std::unique_ptr<SchemaNode> child;
};

struct SchemaNode {
    SchemaNode();
    SchemaNode(const SchemaNode &other);
    SchemaNode& operator=(const SchemaNode &other);
    SchemaNode(SchemaNode &&) noexcept = default;
    SchemaNode& operator=(SchemaNode &&) noexcept = default;

    bool allow_additional = false;
    std::unordered_map<std::string, SchemaProperty> properties;
};

enum class SchemaKind {
    Path,
    Role
};

struct SchemaDefinition {
    SchemaKind kind;
    std::string path;
    std::string role_type;
    SchemaNode root;
};

class SchemaBuilder
{
  public:
    explicit SchemaBuilder(SchemaNode &node);

    SchemaBuilder& allow_additional(bool allow);

    SchemaBuilder& required_string(const std::string &key);
    SchemaBuilder& optional_string(const std::string &key);
    SchemaBuilder& enum_string(const std::string &key, const std::vector<std::string> &values, bool required);

    SchemaBuilder& required_bool(const std::string &key);
    SchemaBuilder& optional_bool(const std::string &key);

    SchemaBuilder& required_int(const std::string &key);
    SchemaBuilder& optional_int(const std::string &key);

    SchemaBuilder& required_uint(const std::string &key);
    SchemaBuilder& optional_uint(const std::string &key);

    SchemaBuilder& required_object(const std::string &key, const std::function<void(SchemaBuilder&)> &fn);
    SchemaBuilder& optional_object(const std::string &key, const std::function<void(SchemaBuilder&)> &fn);

  private:
    SchemaBuilder& add_property(const std::string &key,
                                SchemaValueType type,
                                bool required,
                                std::vector<std::string> enums = {},
                                std::unique_ptr<SchemaNode> child = nullptr);

    SchemaNode &m_node;
};

} // namespace config

class ConfigSchema
{
  public:
    explicit ConfigSchema(config::SchemaDefinition def);

    const config::SchemaDefinition& definition() const;

  private:
    config::SchemaDefinition m_definition;
};

namespace Schema {

ConfigSchema object(const std::string &path, const std::function<void(config::SchemaBuilder&)> &fn);
ConfigSchema role(const std::string &type, const std::function<void(config::SchemaBuilder&)> &fn);

} // namespace Schema

