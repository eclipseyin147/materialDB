#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <nlohmann/json.hpp>

#include "material.h"

namespace CFD_MaterialDB {

    struct PropertyPair;

    struct PropertyValue {
        std::string string_value;
        double numeric_value = 0.0;
        bool is_numeric = false;
        bool is_none = false;
        bool has_nested = false;
        std::shared_ptr<std::vector<PropertyPair>> nested_properties;

        PropertyValue() : nested_properties(std::make_shared<std::vector<PropertyPair>>()) {}
    };


    struct PropertyPair {
        std::string name;
        PropertyValue value;
    };


    struct MaterialData {
        std::string name;
        std::string type;
        std::vector<PropertyPair> properties;
    };


    class ScmParser {
    public:
        std::vector<Material> parse(const std::string &filename);

    private:
        void processProperties(Material &material, const std::vector<PropertyPair> &properties);

        std::string processNestedProperties(const std::shared_ptr<std::vector<PropertyPair>> &nested_props);
        std::string processNestedProperties(const std::vector<std::pair<std::string, PropertyValue>> &nested_props);

        // Add these method declarations in your ScmParser class:
void processSpeciesProperty(Material &material, const std::shared_ptr<std::vector<PropertyPair>> &species_props);
void processReactionsProperty(Material &material, const std::shared_ptr<std::vector<PropertyPair>> &reactions_props);
    };
}
