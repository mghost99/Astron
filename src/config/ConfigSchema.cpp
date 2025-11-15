#include "config/ConfigSchema.h"

using namespace config;

SchemaProperty::SchemaProperty() :
    type(SchemaValueType::String),
    required(false)
{
}

SchemaProperty::SchemaProperty(const SchemaProperty &other) :
    type(other.type),
    required(other.required),
    enum_values(other.enum_values)
{
    if(other.child) {
        child = std::make_unique<SchemaNode>(*other.child);
    }
}

SchemaProperty& SchemaProperty::operator=(const SchemaProperty &other)
{
    if(this == &other) {
        return *this;
    }
    type = other.type;
    required = other.required;
    enum_values = other.enum_values;
    if(other.child) {
        child = std::make_unique<SchemaNode>(*other.child);
    } else {
        child.reset();
    }
    return *this;
}

SchemaNode::SchemaNode() :
    allow_additional(false)
{
}

SchemaNode::SchemaNode(const SchemaNode &other) :
    allow_additional(other.allow_additional),
    properties(other.properties)
{
}

SchemaNode& SchemaNode::operator=(const SchemaNode &other)
{
    if(this == &other) {
        return *this;
    }
    allow_additional = other.allow_additional;
    properties = other.properties;
    return *this;
}

SchemaBuilder::SchemaBuilder(SchemaNode &node) : m_node(node)
{
}

SchemaBuilder& SchemaBuilder::allow_additional(bool allow)
{
    m_node.allow_additional = allow;
    return *this;
}

SchemaBuilder& SchemaBuilder::add_property(const std::string &key,
                                           SchemaValueType type,
                                           bool required,
                                           std::vector<std::string> enums,
                                           std::unique_ptr<SchemaNode> child)
{
    SchemaProperty prop;
    prop.type = type;
    prop.required = required;
    prop.enum_values = std::move(enums);
    prop.child = std::move(child);
    m_node.properties[key] = std::move(prop);
    return *this;
}

SchemaBuilder& SchemaBuilder::required_string(const std::string &key)
{
    return add_property(key, SchemaValueType::String, true);
}

SchemaBuilder& SchemaBuilder::optional_string(const std::string &key)
{
    return add_property(key, SchemaValueType::String, false);
}

SchemaBuilder& SchemaBuilder::enum_string(const std::string &key, const std::vector<std::string> &values, bool required)
{
    return add_property(key, SchemaValueType::String, required, values);
}

SchemaBuilder& SchemaBuilder::required_bool(const std::string &key)
{
    return add_property(key, SchemaValueType::Bool, true);
}

SchemaBuilder& SchemaBuilder::optional_bool(const std::string &key)
{
    return add_property(key, SchemaValueType::Bool, false);
}

SchemaBuilder& SchemaBuilder::required_int(const std::string &key)
{
    return add_property(key, SchemaValueType::Int, true);
}

SchemaBuilder& SchemaBuilder::optional_int(const std::string &key)
{
    return add_property(key, SchemaValueType::Int, false);
}

SchemaBuilder& SchemaBuilder::required_uint(const std::string &key)
{
    return add_property(key, SchemaValueType::UInt, true);
}

SchemaBuilder& SchemaBuilder::optional_uint(const std::string &key)
{
    return add_property(key, SchemaValueType::UInt, false);
}

SchemaBuilder& SchemaBuilder::required_object(const std::string &key, const std::function<void(SchemaBuilder&)> &fn)
{
    auto child = std::make_unique<SchemaNode>();
    SchemaBuilder nested(*child);
    nested.allow_additional(false);
    fn(nested);
    return add_property(key, SchemaValueType::Object, true, {}, std::move(child));
}

SchemaBuilder& SchemaBuilder::optional_object(const std::string &key, const std::function<void(SchemaBuilder&)> &fn)
{
    auto child = std::make_unique<SchemaNode>();
    SchemaBuilder nested(*child);
    nested.allow_additional(false);
    fn(nested);
    return add_property(key, SchemaValueType::Object, false, {}, std::move(child));
}

ConfigSchema::ConfigSchema(config::SchemaDefinition def) :
    m_definition(std::move(def))
{
}

const config::SchemaDefinition& ConfigSchema::definition() const
{
    return m_definition;
}

namespace Schema {

ConfigSchema object(const std::string &path, const std::function<void(SchemaBuilder&)> &fn)
{
    config::SchemaDefinition def;
    def.kind = config::SchemaKind::Path;
    def.path = path;
    SchemaBuilder builder(def.root);
    builder.allow_additional(false);
    fn(builder);
    return ConfigSchema(std::move(def));
}

ConfigSchema role(const std::string &type, const std::function<void(SchemaBuilder&)> &fn)
{
    config::SchemaDefinition def;
    def.kind = config::SchemaKind::Role;
    def.role_type = type;
    SchemaBuilder builder(def.root);
    builder.allow_additional(false);
    fn(builder);
    return ConfigSchema(std::move(def));
}

} // namespace Schema

