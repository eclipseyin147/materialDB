#include "scm_parser.h"
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <algorithm>
#include <fstream>
#include <iostream> // For cerr
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/fusion/include/at_c.hpp>

namespace x3 = boost::spirit::x3;

// Define coefficient_type_symbols
x3::symbols<coefficientType> coefficient_type_symbols;

void init_symbols() {
    coefficient_type_symbols.add
            ("constant", CONSTANT)
            ("polynomial", polynomialT)
            ("polynomial piecewise-linear", polynomialTPieceLinearT)
            ("polynomial piecewise-polynomial", polynomialTPiecePolyT)
            ("polynomial nasa-9-piecewise-polynomial", nasa9PiecePolyT)
            ("compressible-liquid", compressibleT)
            ("sutherland", sutherlandT)
            ("power-law", powerLawT)
            ("blottner-curve-fit", blottnerT);
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

// 定义错误处理器
struct error_handler {
    template<typename Iterator, typename Exception, typename Context>
    x3::error_handler_result on_error(
            Iterator &first, Iterator const &last,
            Exception const &x, Context const &context) {
        std::cerr << "Error: Expecting " << x.which() << " at: \""
                  << std::string(first, std::min(first + 30, last))
                  << "\"..." << std::endl;
        return x3::error_handler_result::fail;
    }
};

struct debug_handler {
    template<typename Iterator, typename Context>
    void on_success(Iterator const &first, Iterator const &last, Context const &context) {
        std::cout << "Parsed: \"" << std::string(first, last) << "\"" << std::endl;
    }
};

struct parameter_class;
struct simple_parameter_class;
struct chemical_formula_property_class;
struct general_property_class;
struct property_class;
struct material_class;
struct scm_file_class;


auto const symbol = x3::rule<struct symbol_, std::string>{"symbol"}
                            = x3::lexeme[+(x3::alnum | x3::char_("-<>=+_.*/:[]{}") | x3::char_("<>") |
                                           x3::char_("<s>"))];

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


///< Rules for parsing species list

auto const species_list = x3::rule<class species_list_, std::vector<std::string>>{}
                                  = x3::lit("names") >> +symbol;

auto const species_property = x3::rule<class species_property_, Property>{"species_property"}
                                      = (x3::lit("species") >> '(' >> species_list >> ')')[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            prop.name = "species";
            // Create a parameter to hold the species names
            Parameter param;
            param.coeff = CONSTANT;
            param.string_value = "species_list"; // Marker to indicate this is a species list
            // Store the species names in the parameter
            auto species_names = x3::_attr(ctx);
            for (const auto &name: species_names) {
                // For each species name, add a value of -999.0 as a placeholder
                param.values.push_back({-999.0});
                // And store the actual name in an additional string field
                if (!param.string_value.empty()) {
                    param.string_value += " ";
                }
                param.string_value += name;
            }

            prop.parameters.push_back(param);
            std::cout << "Parsed species property with " << species_names.size() << " species" << std::endl;
        })];


///< rules for polynomial types
// Rule for parsing a single polynomial piece (a vector of coefficients)
auto const poly_piece = x3::rule<class poly_piece_, std::vector<double>>{}
                                = '(' >> +x3::double_ >> ')';

// Rule for parsing a piecewise polynomial (a vector of polynomial pieces)
auto const piecewise_poly = x3::rule<class piecewise_poly_, std::vector<std::vector<double>>>{}
                                    = +poly_piece;

// Rule for parsing a NASA-9 piecewise polynomial
auto const nasa9_poly = x3::rule<class nasa9_poly_, std::vector<std::vector<double>>>{}
                                = +poly_piece;


auto const comment = x3::lexeme[';' >> *(x3::char_ - x3::eol) >> (x3::eol | x3::eoi)];


auto const parameter = x3::rule<parameter_class, Parameter>{"parameter"}
                               = '('
                >> coefficient_type_symbols[([](auto &ctx) {
                    auto &param = x3::_val(ctx);
                    param.coeff = x3::_attr(ctx);
                    std::cout << "Parsed parameter with coefficient type: " << static_cast<int>(param.coeff)
                              << std::endl;
                })]
                >> (
                        // 处理点后面的简单值
                        ('.' >> (x3::double_[([](auto &ctx) {
                            auto &param = x3::_val(ctx);
                            param.values = {{x3::_attr(ctx)}};
                            std::cout << "Parsed parameter with double value: " << x3::_attr(ctx) << std::endl;
                        })] | symbol[([](auto &ctx) {
                            auto &param = x3::_val(ctx);
                            param.values = {{-999.0}};
                            param.string_value = x3::_attr(ctx);
                            std::cout << "Parsed parameter with symbol value: " << x3::_attr(ctx) << std::endl;
                        })] | boolean[([](auto &ctx) {
                            auto &param = x3::_val(ctx);
                            param.string_value = x3::_attr(ctx) ? "#t" : "#f";
                            param.values = {{x3::_attr(ctx) ? 1.0 : 0.0}};
                            std::cout << "Parsed parameter with boolean value: " << (x3::_attr(ctx) ? "#t" : "#f")
                                      << std::endl;
                        })]))
                        // 处理多项式类型 - 先处理可能的子类型关键字，然后解析多项式数据
                        | ((x3::lit("piecewise-polynomial") | x3::lit("nasa-9-piecewise-polynomial")) >>
                                                                                                      (+poly_piece)[([](
                                                                                                              auto &ctx) {
                                                                                                          auto &param = x3::_val(
                                                                                                                  ctx);
                                                                                                          param.values = x3::_attr(
                                                                                                                  ctx);
                                                                                                          std::cout
                                                                                                                  << "Parsed polynomial structure with "
                                                                                                                  << param.values.size()
                                                                                                                  << " pieces"
                                                                                                                  << std::endl;
                                                                                                      })])
                        // 处理普通数值列表 - 用于 compressible-liquid 等类型
                        | (+x3::double_)[([](auto &ctx) {
                            auto &param = x3::_val(ctx);
                            auto values = x3::_attr(ctx);
                            param.values = {values};
                            std::cout << "Parsed parameter with " << values.size() << " values" << std::endl;
                        })]
                        // 处理嵌套的多项式结构 - 不需要子类型关键字
                        | (+poly_piece)[([](auto &ctx) {
                            auto &param = x3::_val(ctx);
                            param.values = x3::_attr(ctx);
                            std::cout << "Parsed nested polynomial structure with " << param.values.size() << " pieces"
                                      << std::endl;
                        })]
                        // 处理任意内容 - 作为最后的备选方案
                        | (x3::raw[*(x3::char_ - ')')][([](auto &ctx) {
                            auto &param = x3::_val(ctx);
                            auto range = x3::_attr(ctx);
                            std::string content(range.begin(), range.end());
                            // 确保参数至少有一个空的值向量
                            if (param.values.empty()) {
                                param.values.push_back({});
                            }
                            std::cout << "Parsed complex parameter content: " << content.substr(0, 20) << "..."
                                      << std::endl;
                        })])
                )
                >> ')';

// 定义简化参数规则

auto const simple_parameter = x3::rule<simple_parameter_class, Parameter>{"simple_parameter"}
                                      = ('.' >> (x3::double_[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTANT;
            param.values = {{x3::_attr(ctx)}};
            std::cout << "Parsed simple parameter with double value: " << x3::_attr(ctx) << std::endl;
        })] | symbol[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTANT;
            param.values = {{-999.0}};
            param.string_value = x3::_attr(ctx);
            std::cout << "Parsed simple parameter with symbol value: " << x3::_attr(ctx) << std::endl;
        })] | boolean[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTANT;
            param.string_value = x3::_attr(ctx) ? "#t" : "#f";
            param.values = {{x3::_attr(ctx) ? 1.0 : 0.0}};
            std::cout << "Parsed simple parameter with boolean value: " << (x3::_attr(ctx) ? "#t" : "#f") << std::endl;
        })]))
                                        | ('(' >> coefficient_type_symbols >> *x3::double_ >> ')')[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            // In this case, we get a fusion sequence with the coefficient type and a sequence of doubles
            param.coeff = boost::fusion::at_c<0>(x3::_attr(ctx));  // coefficient type

            // Create a vector from the sequence of doubles
            std::vector<double> values;
            auto &doubles = boost::fusion::at_c<1>(x3::_attr(ctx));
            for (auto it = doubles.begin(); it != doubles.end(); ++it) {
                values.push_back(*it);
            }

            param.values = {values}; // Store as a single vector in a vector2d
            std::cout << "Parsed simple parameter with coefficient type and inline values: "
                      << static_cast<int>(param.coeff) << std::endl;
        })];

// 定义化学式属性规则
auto const chemical_formula_property = x3::rule<chemical_formula_property_class, Property>{"chemical_formula_property"}
                                               = (x3::lit("chemical-formula") >> simple_parameter)[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            prop.name = "chemical-formula";
            prop.parameters.push_back(x3::_attr(ctx));
            std::cout << "Parsed chemical formula property with parameter" << std::endl;
        })];


auto const general_property = x3::rule<general_property_class, Property>{"general_property"}
                                      = symbol[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            prop.name = x3::_attr(ctx);
            std::cout << "Setting property name: " << prop.name << std::endl;
        })] >> (
                // 处理任意数量和类型的参数，包括简单参数和复杂参数
                *(simple_parameter[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    prop.parameters.push_back(x3::_attr(ctx));
                    std::cout << "Added simple parameter to property " << prop.name << std::endl;
                })] | parameter[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    prop.parameters.push_back(x3::_attr(ctx));
                    std::cout << "Added parameter to property " << prop.name << std::endl;
                })])
        );

// 定义属性规则
// 定义属性规则
auto const property = x3::rule<property_class, Property>{"property"}
                              = '(' >> (
                (x3::lit("chemical-formula") >> simple_parameter)[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    prop.name = "chemical-formula";
                    prop.parameters.push_back(x3::_attr(ctx));
                    std::cout << "Parsed chemical formula property with parameter" << std::endl;
                })]
                | species_property[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    const Property &parsed = x3::_attr(ctx);
                    prop.name = parsed.name;
                    prop.parameters = parsed.parameters;
                    std::cout << "Parsed species property with " << prop.parameters.size() << " parameters"
                              << std::endl;
                })]
                | (symbol >> *parameter)[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    auto &attr = x3::_attr(ctx);
                    prop.name = boost::fusion::at_c<0>(attr);
                    auto &params = boost::fusion::at_c<1>(attr);
                    for (auto it = params.begin(); it != params.end(); ++it) {
                        prop.parameters.push_back(*it);
                    }
                    std::cout << "Parsed property: " << prop.name
                              << " with " << prop.parameters.size() << " parameters" << std::endl;
                })]
        ) >> ')';

// 定义材料规则
// 定义材料规则
auto const material = x3::rule<material_class, MaterialData>{"material"}
                              = '(' >> symbol[([](auto &ctx) {
            auto &mat = x3::_val(ctx);
            mat.name = x3::_attr(ctx);
            std::cout << "Parsed material name: " << mat.name << std::endl;
        })] >> symbol[([](auto &ctx) {
            auto &mat = x3::_val(ctx);
            mat.type = x3::_attr(ctx);
            std::cout << "Parsed material type: " << mat.type << std::endl;
        })] >> *(
                property[([](auto &ctx) {
                    auto &mat = x3::_val(ctx);
                    const Property &prop = x3::_attr(ctx);
                    mat.properties.push_back(prop);
                    std::cout << "Added property " << prop.name << " to material " << mat.name << std::endl;
                })]
                | comment
        ) >> ')';

// 定义 SCM 文件规则
auto const scm_file = x3::rule<scm_file_class, std::vector<MaterialData>>{"scm_file"}
                              = x3::skip(comment | x3::space)[*material];

// 将定义与声明关联起来

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
        // 添加日志，输出文件内容前几行
        std::cout << "Parsing file: " << filename << std::endl;
        std::string preview = content.substr(0, std::min(size_t(200), content.size()));
        std::cout << "File preview: " << std::endl << preview << "..." << std::endl;

        bool success = x3::phrase_parse(iter, end, scm_file, x3::space, parsed_materials);

        // 添加日志，输出解析结果
        std::cout << "Parse success: " << success << std::endl;
        std::cout << "Parsed materials count: " << parsed_materials.size() << std::endl;
        std::cout << "Remaining unparsed: " << (iter != end) << std::endl;
        if (iter != end) {
            std::cout << "Remaining text: '" << std::string(iter, std::min(iter + 50, end)) << "...'" << std::endl;
        }

        if (!success || iter != end) {
            std::cerr << "解析失败 at: '" << std::string(iter, std::min(iter + 20, end)) << "...'"
                      << std::endl;
            return materials_out;
        }

        // 添加日志，输出每个解析到的材料
        for (size_t i = 0; i < parsed_materials.size(); ++i) {
            std::cout << "Material " << i << ": " << parsed_materials[i].name
                      << ", Type: " << parsed_materials[i].type
                      << ", Properties count: " << parsed_materials[i].properties.size() << std::endl;
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

        // In the processProperties method, add a new case for species
        if (key == "species" && !prop.parameters.empty()) {
            const auto &param = prop.parameters[0];
            if (param.string_value.find("species_list") == 0) {
                // Extract species names from the string_value
                std::string species_str = param.string_value.substr(12); // Skip "species_list "
                std::istringstream iss(species_str);
                std::vector<std::string> species_names;
                std::string name;
                while (iss >> name) {
                    species_names.push_back(name);
                }

                // Create a MaterialProperty for the species list
                // Create a MaterialProperty for the species list
                MaterialProperty mp;
                mp.name = "species";
                mp.unit = "";
                mp.type = "string_array";
                mp.data = species_names;
                material.properties["species"] = mp;

                // Also store the species names in the material's species field
                material.speciesName = species_names;
            }
            continue;
        }

        if (key == "density" || key == "specific-heat-ratio" || key == "molecular-weight") {
            const auto &param = prop.parameters[0];
            if (param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1 &&
                param.values[0][0] == -999.0 && !param.string_value.empty()) {
                // 使用 MaterialProperty 结构
                MaterialProperty mp;
                mp.name = key;
                mp.unit = "";
                mp.type = "string";
                mp.data = param.string_value;
                material.properties[key] = mp;

            } else if (param.coeff == polynomialTPieceLinearT) {
                // 使用 MaterialProperty 结构
                MaterialProperty mp;
                mp.name = key;
                mp.unit = "";
                mp.type = "string";
                mp.data = param.string_value;
                material.properties[key] = mp;
            } else if (param.coeff == polynomialTPiecePolyT) {
                // 使用 MaterialProperty 结构
                MaterialProperty mp;
                mp.name = key;
                mp.unit = "";
                mp.type = "string";
                mp.data = param.string_value;
                material.properties[key] = mp;
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
                    } else if (param.coeff == nasa9PiecePolyT) {
                        mp.type = "nasa_polynomial";
                        NASAPolynomialData nasaData;
                        // Populate NASA polynomial data
                        // This is a placeholder, actual implementation would depend on data format
                        mp.data = nasaData;
                    } else {  // polynomialTPieceLinearT or polynomialTPiecePolyT
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