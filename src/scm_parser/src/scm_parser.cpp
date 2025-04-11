#include "scm_parser.h"
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>
// #include <boost/phoenix.hpp> // Likely not needed with x3 semantic actions
// #include <boost/phoenix/core.hpp>
// #include <boost/phoenix/operator.hpp>
// #include <boost/phoenix/bind/bind_member_function.hpp>
#include <algorithm>
#include <fstream>
#include <iostream> // For cerr

#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
namespace x3 = boost::spirit::x3;

// Define coefficient_type_symbols
x3::symbols<coefficientType> coefficient_type_symbols;
void init_symbols() {
    coefficient_type_symbols.add
            ("constant", CONSTANT)
            ("polynomial", polynomialT)
            ("polynomial piecewise-linear", polynomialTPieceLinearT)
            ("polynomial piecewise-polynomial", polynomialTPiecePolyT)
            ("polynomial nasa-9-piecewise-polynomial", nasa9PiecePolyT);
}

BOOST_FUSION_ADAPT_STRUCT(
        Parameter,
        coeff,              // coefficientType
        values,             // std::vector<std::vector<double>>
        string_value
)

BOOST_FUSION_ADAPT_STRUCT(
        Property,
        name,               // std::string
        parameters          // std::vector<Parameter>
)

BOOST_FUSION_ADAPT_STRUCT(
    MaterialData,
    name,
    type,
    properties
)

auto const symbol = x3::rule<struct symbol_, std::string>{"symbol"}
                            = x3::lexeme[+(x3::alnum | x3::char_("-<>=+_.")|x3::char_("<>")|x3::char_("<s>"))];
auto const string_lit = x3::rule<struct string_, std::string>{"string"}
    = x3::lexeme['"' >> *(('\\' >> x3::char_) | ~x3::char_('"')) >> '"'];
auto const boolean = x3::rule<struct boolean_, bool>{"boolean"}
    = x3::lit("#t")[([](auto &ctx) { x3::_val(ctx) = true; })]
    | x3::lit("#f")[([](auto &ctx) { x3::_val(ctx) = false; })];
auto const number = x3::rule<struct number_, double>{"number"}
    = x3::double_ | x3::int_;
auto const vector1d = x3::rule<class vector1d_, std::vector<double>>{}
    = '(' >> *x3::double_ >> ')';
auto const vector2d = x3::rule<class vector2d_, std::vector<std::vector<double>>>{}
    = '(' >> *vector1d >> ')';

auto const parameter = x3::rule<class parameter_, Parameter>{"parameter"}
    = '('
    >> coefficient_type_symbols
    >> (vector2d | ('.' >> (x3::double_[([](auto &ctx) {
        auto &param = x3::_val(ctx);
        param.values = {{x3::_attr(ctx)}};
    })] | symbol[([](auto &ctx) {
        auto &param = x3::_val(ctx);
        param.values = {{-999.0}};
        param.string_value = x3::_attr(ctx);
    })] | boolean[([](auto &ctx) {
        auto &param = x3::_val(ctx);
        param.string_value = x3::_attr(ctx) ? "#t" : "#f";
        param.values = {{x3::_attr(ctx) ? 1.0 : 0.0}};
    })])))
    >> ')';

auto const chemical_formula_property_action = [](auto& ctx)
        {
    std::string formula_str = x3::_attr(ctx);
    Property prop;
    prop.name = "chemical-formula";

    Parameter param;
    param.coeff = CONSTANT;
    param.values = {{-999.0}};
    param.string_value = formula_str;

    prop.parameters.push_back(param);

    x3::_val(ctx) = prop;
};

auto const chemical_formula_property = x3::rule<class cfp_, Property>{"chemical_formula_property"}
    = (x3::lit("chemical-formula") >> '.' >> symbol)[chemical_formula_property_action];

auto const general_property = x3::rule<class gp_, Property>{"general_property"}
    = symbol >> *parameter;

auto const property = x3::rule<struct property_, Property>{"property"}
    = '(' >> (chemical_formula_property | general_property) >> ')';

auto const material = x3::rule<struct material_, MaterialData>{"material"}
    = '(' >> symbol >> symbol >> *property >> ')';

auto const comment = x3::lexeme[';' >> *(x3::char_ - x3::eol) >> x3::eol];
auto const scm_file = x3::rule<struct scm_file_, std::vector<MaterialData>>{"scm_file"}
    = x3::skip(comment | x3::space)[*material];

std::vector<Material> ScmParser::parse(const std::string &filename) {
    std::vector<Material> materials_out;
    init_symbols(); // Initialize symbol table

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return materials_out;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    std::vector<MaterialData> parsed_materials;
    auto iter = content.begin();
    auto end = content.end();
    try {
        bool success = x3::phrase_parse(iter, end, scm_file, x3::space, parsed_materials);
        if (!success || iter != end) {
            std::cerr << "解析失败 at: '" << std::string(iter, std::min(iter + 20, end)) << "...'"
                      << std::endl;
            return materials_out;
        }

        for (const auto &mat_data: parsed_materials) {
            Material material;
            material.name = mat_data.name;
            std::string typeStr = mat_data.type;
            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);

            if (typeStr == "FLUID") {
                material.type.state = MaterialState::FLUID;
            } else if (typeStr == "SOLID") {
                material.type.state = MaterialState::SOLID;
            } else if (typeStr == "MIXTURE") {
                material.type.state = MaterialState::MIXTURE;
            }

            processProperties(material, mat_data);
            materials_out.push_back(material);
        }
    } catch (const x3::expectation_failure<std::string::iterator> &e) {
        std::cerr << "Parse expectation failure near '"
                  << std::string(e.where(), std::min(e.where() + 20, content.end())) << "': Expected "
                  << e.which() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "解析异常: " << e.what() << std::endl;
    }

    return materials_out;
}

void ScmParser::processProperties(Material &material, const MaterialData &mat_data) {
    for (const auto &prop: mat_data.properties) {
        const auto &key = prop.name;

        // 处理化学式
        if (key == "chemical-formula" && !prop.parameters.empty()) {
            const auto &param = prop.parameters[0];
            if (param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1 &&
                param.values[0][0] == -999.0 && !param.string_value.empty()) {
                // 使用 MaterialProperty 结构
                MaterialProperty mp;
                mp.name = "chemical-formula";
                mp.unit = "";
                mp.type = "string";
                mp.data = param.string_value;
                material.properties["chemical-formula"] = mp;

                // 设置 chemical_formula 字段
                material.chemical_formula = param.string_value;
            } else if (param.string_value == "#f" || param.string_value == "#t") {
                // 处理 (chemical-formula . #f) 这种情况
                MaterialProperty mp;
                mp.name = "chemical-formula";
                mp.unit = "";
                mp.type = "boolean";
                mp.data = (param.string_value == "#t") ? 1.0 : 0.0;
                material.properties["chemical-formula"] = mp;
            }
            continue;
        }

        if (!prop.parameters.empty()) {
            for (const auto &param: prop.parameters) {
                bool processed = false;

                // 创建新的 MaterialProperty
                MaterialProperty mp;
                mp.name = key;
                mp.unit = ""; // Unit should be set based on property type if known

                // 处理布尔值参数
                if (param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1 &&
                    (param.values[0][0] == 1.0 || param.values[0][0] == 0.0) && !param.string_value.empty() &&
                    (param.string_value == "#t" || param.string_value == "#f")) {
                    mp.type = "boolean";
                    mp.data = (param.values[0][0] == 1.0) ? 1.0 : 0.0;
                    material.properties[key] = mp;
                    processed = true;
                }
                // 处理字符串参数
                else if (param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1 &&
                         param.values[0][0] == -999.0 && !param.string_value.empty()) {
                    mp.type = "string";
                    mp.data = param.string_value;
                    material.properties[key] = mp;
                    processed = true;
                }
                // 处理简单数值参数
                else if (!processed && param.coeff == CONSTANT && param.values.size() == 1 &&
                         param.values[0].size() == 1) {
                    mp.type = "number";
                    mp.data = param.values[0][0];
                    material.properties[key] = mp;
                    processed = true;
                }
                // 处理多项式或分段多项式参数
                else if (!processed && (param.coeff == polynomialT ||
                         param.coeff == polynomialTPieceLinearT ||
                         param.coeff == polynomialTPiecePolyT ||
                         param.coeff == nasa9PiecePolyT)) {

                    if (param.coeff == polynomialT) {
                        mp.type = "polynomial";
                        PolynomialData polyData;
                        if (!param.values.empty()) {
                            polyData.coefficients = param.values[0];
                            polyData.min_temp = 0.0;  // Default values, should be updated if available
                            polyData.max_temp = 0.0;
                            mp.data = polyData;
                        }
                    }
                    else if (param.coeff == nasa9PiecePolyT) {
                        mp.type = "nasa_polynomial";
                        NASAPolynomialData nasaData;
                        // Populate NASA polynomial data
                        // This is a placeholder, actual implementation would depend on data format
                        mp.data = nasaData;
                    }
                    else {  // polynomialTPieceLinearT or polynomialTPiecePolyT
                        mp.type = "piecewise_polynomial";
                        PiecewisePolynomialData piecewiseData;
                        piecewiseData.coefficients = param.values;
                        // Temperature ranges would need to be populated based on data format
                        mp.data = piecewiseData;
                    }

                    material.properties[key] = mp;
                    processed = true;
                }

                if (processed) break; // 已处理第一个参数，跳出循环
            }
        } else {
            // 无参数的属性，视为标志属性
            MaterialProperty mp;
            mp.name = key;
            mp.unit = "";
            mp.type = "boolean";
            mp.data = 1.0;  // true as double
            material.properties[key] = mp;
        }
    }
}