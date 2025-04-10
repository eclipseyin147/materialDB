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
    polynomialT,
    polynomialTPieceLinearT,                 ///< std::vector<double>
    polynomialTPiecePolyT,   ///< std::vector<std::array<double, 7>>
    nasa9PiecePolyT   ///< std::vector<std::array<double, 9>>
};



void init_symbols() ;


// 属性参数：支持单值或列表
struct Parameter {
    coefficientType coeff;
    std::vector<std::vector<double>> values;
    std::string string_value; // 用于存储字符串值，如化学式
};

// 属性定义：如 (density (constant 1.225))
struct Property {
    std::string name;
    std::vector<Parameter> parameters;
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


