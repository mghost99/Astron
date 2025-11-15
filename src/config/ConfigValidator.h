#pragma once

#include "config/ConfigSchema.h"
#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

class ConfigValidator
{
  public:
    explicit ConfigValidator(const YAML::Node &root);

    void register_schema(const ConfigSchema &schema);

    bool validate(std::vector<std::string> &errors) const;

  private:
    YAML::Node m_root;
    std::vector<ConfigSchema> m_schemas;

    void validate_definition(const ConfigSchema &schema, std::vector<std::string> &errors) const;
    void validate_role(const ConfigSchema &schema, std::vector<std::string> &errors) const;
    void validate_path(const ConfigSchema &schema, std::vector<std::string> &errors) const;

    static bool validate_node(const YAML::Node &node,
                              const config::SchemaNode &schema,
                              const std::string &path,
                              std::vector<std::string> &errors);
    static bool validate_property(const YAML::Node &node,
                                  const config::SchemaProperty &prop,
                                  const std::string &path,
                                  std::vector<std::string> &errors);
};

