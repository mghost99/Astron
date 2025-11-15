#include "config/ConfigValidator.h"

#include <sstream>

using namespace config;

ConfigValidator::ConfigValidator(const YAML::Node &root) :
    m_root(root)
{
}

void ConfigValidator::register_schema(const ConfigSchema &schema)
{
    m_schemas.push_back(schema);
}

bool ConfigValidator::validate(std::vector<std::string> &errors) const
{
    for(const auto &schema : m_schemas) {
        validate_definition(schema, errors);
    }
    return errors.empty();
}

void ConfigValidator::validate_definition(const ConfigSchema &schema, std::vector<std::string> &errors) const
{
    const SchemaDefinition &def = schema.definition();
    if(def.kind == SchemaKind::Role) {
        validate_role(schema, errors);
    } else {
        validate_path(schema, errors);
    }
}

void ConfigValidator::validate_role(const ConfigSchema &schema, std::vector<std::string> &errors) const
{
    const SchemaDefinition &def = schema.definition();
    YAML::Node roles = m_root["roles"];
    if(!roles || !roles.IsSequence()) {
        errors.push_back("Config: 'roles' must be a list.");
        return;
    }

    bool found = false;
    for(std::size_t idx = 0; idx < roles.size(); ++idx) {
        YAML::Node entry = roles[idx];
        if(!entry["type"]) {
            std::stringstream ss;
            ss << "Config: roles[" << idx << "] missing 'type' attribute.";
            errors.push_back(ss.str());
            continue;
        }
        std::string type = entry["type"].as<std::string>();
        if(type != def.role_type) {
            continue;
        }
        found = true;
        std::stringstream path;
        path << "$.roles[" << idx << "](" << type << ")";
        validate_node(entry, def.root, path.str(), errors);
    }

    if(!found) {
        std::stringstream ss;
        ss << "Config: No role with type '" << def.role_type << "' found in roles list.";
        errors.push_back(ss.str());
    }
}

void ConfigValidator::validate_path(const ConfigSchema &schema, std::vector<std::string> &errors) const
{
    const SchemaDefinition &def = schema.definition();
    YAML::Node node = m_root;
    std::stringstream path;
    path << "$";

    std::size_t start = 0;
    std::string name = def.path;
    while(start < name.size()) {
        std::size_t dot = name.find('.', start);
        std::string part = dot == std::string::npos ? name.substr(start) : name.substr(start, dot - start);
        node = node[part];
        path << "." << part;
        if(!node) {
            std::stringstream ss;
            ss << "Config: Expected section '" << def.path << "', but '" << path.str().substr(2)
               << "' is missing.";
            errors.push_back(ss.str());
            return;
        }
        start = dot == std::string::npos ? name.size() : dot + 1;
    }

    validate_node(node, def.root, path.str(), errors);
}

bool ConfigValidator::validate_node(const YAML::Node &node,
                                    const SchemaNode &schema,
                                    const std::string &path,
                                    std::vector<std::string> &errors)
{
    if(!node || !node.IsDefined()) {
        errors.emplace_back(path + " is missing.");
        return false;
    }
    if(!node.IsMap()) {
        errors.emplace_back(path + " must be a mapping.");
        return false;
    }

    bool ok = true;

    for(const auto &[name, prop] : schema.properties) {
        YAML::Node value = node[name];
        std::string child_path = path + "." + name;
        if(!value) {
            if(prop.required) {
                errors.emplace_back(child_path + " is required.");
                ok = false;
            }
            continue;
        }
        if(!validate_property(value, prop, child_path, errors)) {
            ok = false;
        }
    }

    if(!schema.allow_additional) {
        for(auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.as<std::string>();
            if(schema.properties.find(key) == schema.properties.end()) {
                errors.emplace_back(path + "." + key + " is not a recognized attribute.");
                ok = false;
            }
        }
    }

    return ok;
}

bool ConfigValidator::validate_property(const YAML::Node &node,
                                        const SchemaProperty &prop,
                                        const std::string &path,
                                        std::vector<std::string> &errors)
{
    switch(prop.type) {
    case SchemaValueType::String:
        if(!node.IsScalar()) {
            errors.emplace_back(path + " must be a string.");
            return false;
        }
        if(!prop.enum_values.empty()) {
            std::string val = node.as<std::string>();
            bool match = false;
            for(const auto &allowed : prop.enum_values) {
                if(val == allowed) {
                    match = true;
                    break;
                }
            }
            if(!match) {
                std::stringstream ss;
                ss << path << " must be one of [";
                for(std::size_t i = 0; i < prop.enum_values.size(); ++i) {
                    ss << prop.enum_values[i];
                    if(i + 1 < prop.enum_values.size()) {
                        ss << ", ";
                    }
                }
                ss << "].";
                errors.push_back(ss.str());
                return false;
            }
        }
        return true;
    case SchemaValueType::Bool:
        if(!node.IsScalar() && !node.IsSequence() && !node.IsMap()) {
            // YAML bools may be scalar only
        }
        try {
            (void)node.as<bool>();
            return true;
        } catch(const std::exception &) {
            errors.emplace_back(path + " must be a boolean.");
            return false;
        }
    case SchemaValueType::Int:
        try {
            (void)node.as<long long>();
            return true;
        } catch(const std::exception &) {
            errors.emplace_back(path + " must be an integer.");
            return false;
        }
    case SchemaValueType::UInt:
        try {
            (void)node.as<unsigned long long>();
            return true;
        } catch(const std::exception &) {
            errors.emplace_back(path + " must be an unsigned integer.");
            return false;
        }
    case SchemaValueType::Number:
        try {
            (void)node.as<double>();
            return true;
        } catch(const std::exception &) {
            errors.emplace_back(path + " must be numeric.");
            return false;
        }
    case SchemaValueType::Object:
        if(!prop.child) {
            errors.emplace_back(path + " schema missing child definition.");
            return false;
        }
        return validate_node(node, *prop.child, path, errors);
    }
    return true;
}

