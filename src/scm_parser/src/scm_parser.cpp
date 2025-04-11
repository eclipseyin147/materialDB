#include "scm_parser.h"
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
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
    template <typename Iterator, typename Exception, typename Context>
    x3::error_handler_result on_error(
            Iterator& first, Iterator const& last,
            Exception const& x, Context const& context) {
        std::cerr << "Error: Expecting " << x.which() << " at: \""
                  << std::string(first, std::min(first + 30, last))
                  << "\"..." << std::endl;
        return x3::error_handler_result::fail;
    }
};

struct debug_handler {
    template <typename Iterator, typename Context>
    void on_success(Iterator const& first, Iterator const& last, Context const& context) {
        std::cout << "Parsed: \"" << std::string(first, last) << "\"" << std::endl;
    }
};




auto const symbol = x3::rule<struct symbol_, std::string>{"symbol"}
                            = x3::lexeme[+(x3::alnum | x3::char_("-<>=+_.*/:[]{}()") | x3::char_("<>") | x3::char_("<s>"))];

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


// 首先声明所有规则
struct parameter_class;
auto const parameter = x3::rule<parameter_class, Parameter>{"parameter"};

struct simple_parameter_class;
auto const simple_parameter = x3::rule<simple_parameter_class, Parameter>{"simple_parameter"};

struct chemical_formula_property_class;
auto const chemical_formula_property = x3::rule<chemical_formula_property_class, Property>{"chemical_formula_property"};

struct general_property_class;
auto const general_property = x3::rule<general_property_class, Property>{"general_property"};

struct property_class;
auto const property = x3::rule<property_class, Property>{"property"};

struct material_class;
auto const material = x3::rule<material_class, MaterialData>{"material"};

// 然后定义规则
auto const parameter_def =
    '('
    >> coefficient_type_symbols
    >> (vector2d | ('.' >> (x3::double_[([](auto &ctx) {
        auto &param = x3::_val(ctx);
        param.coeff = CONSTANT;
        param.values = {{x3::_attr(ctx)}};
        std::cout << "Parsed parameter with double value: " << x3::_attr(ctx) << std::endl;
    })] | symbol[([](auto &ctx) {
        auto &param = x3::_val(ctx);
        param.coeff = CONSTANT;
        param.values = {{-999.0}};
        param.string_value = x3::_attr(ctx);
        std::cout << "Parsed parameter with symbol value: " << x3::_attr(ctx) << std::endl;
    })] | boolean[([](auto &ctx) {
        auto &param = x3::_val(ctx);
        param.coeff = CONSTANT;
        param.string_value = x3::_attr(ctx) ? "#t" : "#f";
        param.values = {{x3::_attr(ctx) ? 1.0 : 0.0}};
        std::cout << "Parsed parameter with boolean value: " << (x3::_attr(ctx) ? "#t" : "#f") << std::endl;
    })])))
    >> ')';

// 添加一个简化的参数规则，用于处理 (chemical-formula . #f) 这样的格式
auto const simple_parameter_def =
    '.' >> (x3::double_[([](auto &ctx) {
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
    })]);

auto const chemical_formula_property_def =
    (x3::lit("chemical-formula") >> simple_parameter)[([](auto &ctx) {
        auto &prop = x3::_val(ctx);
        prop.name = "chemical-formula";
        prop.parameters.push_back(x3::_attr(ctx));
        std::cout << "Parsed chemical formula property with parameter" << std::endl;
    })];

auto const general_property_def =
    symbol[([](auto &ctx) {
        auto& prop = x3::_val(ctx);
        prop.name = x3::_attr(ctx);
        std::cout << "Setting property name: " << prop.name << std::endl;
    })] >> (simple_parameter[([](auto &ctx) {
        auto& prop = x3::_val(ctx);
        prop.parameters.push_back(x3::_attr(ctx));
        std::cout << "Added simple parameter to property " << prop.name << std::endl;
    })] | *parameter[([](auto &ctx) {
        auto& prop = x3::_val(ctx);
        prop.parameters.push_back(x3::_attr(ctx));
        std::cout << "Added parameter to property " << prop.name << std::endl;
    })]);

auto const property_def =
    '(' >> (chemical_formula_property[([](auto &ctx) {
        const Property& prop = x3::_attr(ctx);
        std::cout << "Parsed chemical formula property: " << prop.name
                  << " with " << prop.parameters.size() << " parameters" << std::endl;
    })] | general_property[([](auto &ctx) {
        const Property& prop = x3::_attr(ctx);
        std::cout << "Parsed general property: " << prop.name
                  << " with " << prop.parameters.size() << " parameters" << std::endl;
    })]) >> ')';

auto const material_def =
    '(' >> symbol[([](auto &ctx) {
        auto& mat = x3::_val(ctx);
        mat.name = x3::_attr(ctx);
        std::cout << "Parsed material name: " << mat.name << std::endl;
    })] >> symbol[([](auto &ctx) {
        auto& mat = x3::_val(ctx);
        mat.type = x3::_attr(ctx);
        std::cout << "Parsed material type: " << mat.type << std::endl;
    })] >> *property[([](auto &ctx) {
        auto& mat = x3::_val(ctx);
        const Property& prop = x3::_attr(ctx);
        mat.properties.push_back(prop);
        std::cout << "Added property " << prop.name << " to material " << mat.name << std::endl;
    })] >> ')';

// 最后将定义与声明关联起来
BOOST_SPIRIT_DEFINE(parameter, simple_parameter, chemical_formula_property, general_property, property, material);


auto const comment = x3::lexeme[';' >> *(x3::char_ - x3::eol) >> (x3::eol | x3::eoi)];

///< SCM file rule
auto const scm_file_def = x3::skip(comment | x3::space)[*material[([](auto &ctx) {
    const auto& mat = x3::_attr(ctx);
    std::cout << "Successfully parsed material: " << mat.name
              << " with " << mat.properties.size() << " properties" << std::endl;
    x3::_val(ctx).push_back(mat);
})]];

struct scm_file_class;
auto const scm_file = x3::rule<scm_file_class, std::vector<MaterialData>>{"scm_file"} = scm_file_def;



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

        if (!success || iter != end) {
            std::cerr << "解析失败 at: '" << std::string(iter, std::min(iter + 20, end)) << "...'"
                      << std::endl;
            return materials_out;
        }

        // 添加日志，输出每个解析到的材料
        for (size_t i = 0; i < parsed_materials.size(); ++i) {
            std::cerr << "Material " << i << ": " << parsed_materials[i].name
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