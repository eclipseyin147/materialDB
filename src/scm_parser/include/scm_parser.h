#pragma once

#include <string>
#include <vector>
#include <map>
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
enum coefficientType {
    CONSTANT,
    compressibleT,
    sutherlandT,
    powerLawT,
    blottnerT,
    polynomialT,
    polynomialTPieceLinearT,                 ///< std::vector<double>
    polynomialTPiecePolyT,   ///< std::vector<std::array<double, 7>>
    nasa9PiecePolyT   ///< std::vector<std::array<double, 9>>
};



void init_symbols() ;



struct Parameter {
    coefficientType coeff;
    std::vector<std::vector<double>> values;
    std::string string_value;
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

class ScmParser
        {
public:
    std::vector<Material> parse(const std::string &filename);

private:
    void processProperties(Material &material, const MaterialData &mat_data);
};


