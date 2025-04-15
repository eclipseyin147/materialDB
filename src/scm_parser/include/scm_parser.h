#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <memory>
#include <iostream>
#include <nlohmann/json.hpp>
#include <boost/fusion/adapted.hpp>
#include "material.h"
#include <optional>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/string/symbols.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>

namespace x3 = boost::spirit::x3;


const static std::unordered_set<std::string> propertyTypeNames =
        {
        "density", "specific-heat", "thermal-conductivity", "viscosity", "molecular-weight",
        "absorption-coefficient", "formation-enthalpy", "reference-temperature", "formation-entropy",
        "critical-pressure","latent-heat","vaporization-temperature","boiling-point",
        "binary-diffusivity","volatile-fraction","combustible-fraction",
        "swelling-coefficient","emissivity","scattering-factor",
        "critical-temperature", "critical-volume", "acentric-factor",
        "struct-youngs-modulus", "struct-poisson-ratio"
};

void init_symbols();


struct Parameter {
    coefficientType coeff;
    std::vector<std::vector<double>> values;
    std::string string_value;
    std::unordered_set<std::string> particleTypes;
};


struct Property {
    std::string name;
    std::vector<Parameter> parameters;
};

struct PropertyValue {
    std::string name;
    std::string coefficient_type;
    std::variant<
            bool,
            double,
            std::string,
            std::vector<std::vector<double>>
    > value;

    std::string type;
};


struct MaterialData {
    std::string name;
    std::string type;
    std::optional<std::string> chemical_formula;
    std::vector<Property> properties;
};

using error_handler_type = x3::error_handler<std::string::iterator>;

class ScmParser {
public:
    std::vector<Material> parse(const std::string &filename);

private:
    void processProperties(Material &material, const MaterialData &mat_data);
};


