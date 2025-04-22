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
#include "material.h"

namespace x3 = boost::spirit::x3;

// Define coefficient_type_symbols
x3::symbols<coefficientType> coefficient_type_symbols;
x3::symbols<binaryDiffusModelType> binDiff_type_symbols;
x3::symbols<binaryDiffusModelParam> binDiff_param_symbols;

void init_symbols() {
    coefficient_type_symbols.add
            ("constant", CONSTCOEFF)
            ("polynomial", polynomialT)
            ("polynomial piecewise-linear", polynomialTPieceLinearT)
            ("polynomial piecewise-polynomial", polynomialTPiecePolyT)
            ("polynomial nasa-9-piecewise-polynomial", nasa9PiecePolyT)
            ("compressible-liquid", compressibleT)
            ("sutherland", sutherlandT)
            ("power-law", powerLawT)
            ("blottner-curve-fit", blottnerT)
            ("averaging-coefficient", AVERAGING_COEFF);

    binDiff_type_symbols.add
            ("constant", CONSTANT_DIFFUSION)
            ("film-averaged", FILM_AVERAGED_DIFFUSION);

    binDiff_param_symbols.add
            ("film-diffusivity", FILM_DIFFUSIVITY);
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
                            = x3::lexeme[+(x3::alnum | x3::char_("-<>=+_.*/:[]{},.") | x3::char_("<>") |
                                           x3::char_("<s>") | x3::char_("<l>") | x3::char_("<g>"))];

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
            param.coeff = CONSTCOEFF;
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
// Modified rule to support both formats:
// 1. Regular list of doubles: (1.0 2.0 3.0)
// 2. Temperature-value pair: (87 . 1117)

auto const temp_value_pair = x3::rule<class temp_value_pair_, std::pair<double, double>>{}
                                     = '(' >> x3::double_ >> '.' >> x3::double_ >> ')';

auto const poly_piece = x3::rule<class poly_piece_, std::vector<double>>{}
                                = (
                // Option 1: Parse temp_value_pair format (temp . value)
                temp_value_pair
                [([](auto &ctx) {
                    // Attribute from temp_value_pair is std::pair<double, double>
                    auto pair_attr = x3::_attr(ctx);
                    // Convert pair to vector<double> for poly_piece attribute
                    x3::_val(ctx) = {pair_attr.first, pair_attr.second};
                    std::cout << "Parsed poly_piece as temp-value pair: (" << pair_attr.first << " . "
                              << pair_attr.second << ")" << std::endl;
                })]
                |
                // Option 2: Parse original list of doubles format (double double ...)
                ('(' >> +x3::double_ >> ')')
                [([](auto &ctx) {
                    // Attribute from +x3::double_ is already std::vector<double>
                    x3::_val(ctx) = x3::_attr(ctx);
                    std::cout << "Parsed poly_piece as list of doubles with " << x3::_val(ctx).size() << " elements"
                              << std::endl;
                })]
        );

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
                            std::cout << "######## Parsed nested polynomial structure with " << param.values.size()
                                      << " pieces"
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

auto const nested_parameter = x3::rule<class nested_parameter_, Parameter>{"nested_parameter"}
                                      = parameter[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param = x3::_attr(ctx);  // Just copy the parameter data directly
            std::cout << "Captured nested parameter of type: " << static_cast<int>(param.coeff) << std::endl;
        })];


auto const simple_parameter = x3::rule<simple_parameter_class, Parameter>{"simple_parameter"}
                                      = ('.' >> (x3::double_[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTCOEFF;
            param.values = {{x3::_attr(ctx)}};
            std::cout << "Parsed simple parameter with double value: " << x3::_attr(ctx) << std::endl;
        })] | symbol[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTCOEFF;
            param.values = {{-999.0}};
            param.string_value = x3::_attr(ctx);
            std::cout << "Parsed simple parameter with symbol value: " << x3::_attr(ctx) << std::endl;
        })] | boolean[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTCOEFF;
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


auto const averaging_coefficient_param = x3::rule<class averaging_coefficient_param_, Parameter>{"averaging_coefficient_param"}
    = '(' >> x3::lit("averaging-coefficient") >> x3::double_[([](auto &ctx) {
        auto &param = x3::_val(ctx);
        param.coeff = AVERAGING_COEFF;
        param.string_value = "averaging-coefficient";
        param.values = {{x3::_attr(ctx)}};
        std::cout << "Parsed averaging coefficient: " << x3::_attr(ctx) << std::endl;
    })] >> ')';

auto const film_diffusivity_param = x3::rule<class film_diffusivity_param_, Parameter>{"film_diffusivity_param"}
    = '(' >> x3::lit("film-diffusivity") >>
        (
            // Direct parameter case (like constant)
            parameter[([](auto &ctx) {
                auto &param = x3::_val(ctx);
                param.string_value = "film-diffusivity";
                param.coeff = x3::_attr(ctx).coeff;
                param.values = x3::_attr(ctx).values;
                std::cout << "Parsed film-diffusivity with direct parameter" << std::endl;
            })]
            |
            // Multiple parameters inside parentheses
            (*nested_parameter)[([](auto &ctx) {
                auto &param = x3::_val(ctx);
                param.string_value = "film-diffusivity";

                // Store all nested parameters for later processing
                auto nested_params = x3::_attr(ctx);
                for (const auto &np : nested_params) {
                    // Record the coefficient type of each nested parameter
                    param.particleTypes.insert(std::to_string(static_cast<int>(np.coeff)));

                    // If it's a piecewise-linear polynomial, store the temperature-value pairs
                    if (np.coeff == polynomialTPieceLinearT) {
                        param.values = np.values;  // Store all temp-value pairs
                    } else if (np.coeff == CONSTCOEFF) {
                        // For constant values, add to the end of values
                        param.values.push_back(np.values[0]);
                    }
                }

                std::cout << "Parsed film-diffusivity with " << nested_params.size() << " nested parameters" << std::endl;
            })]
        ) >> ')';

// Rule for film-averaged binary diffusion with its parameters
auto const film_averaged_param = x3::rule<class film_averaged_param_, Parameter>{"film_averaged_param"}
    = '(' >> x3::lit("film-averaged") >> '(' >>
        (
            // Accept both parameters in any order, but make sure they're correctly stored
            *(averaging_coefficient_param | film_diffusivity_param)
        )[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = static_cast<coefficientType>(static_cast<int>(FILM_AVERAGED_DIFFUSION));
            param.string_value = "film-averaged";

            // Process the list of parameters
            auto params = x3::_attr(ctx);
            for (const auto &variant_param : params) {
                // Handle the variant type correctly using boost::get
                const Parameter &p = boost::get<Parameter>(variant_param);

                if (p.string_value == "averaging-coefficient") {
                    // Store the averaging coefficient
                    if (!p.values.empty() && !p.values[0].empty()) {
                        param.values.push_back(p.values[0]);
                    }
                    param.particleTypes.insert("averaging-coefficient");
                }
                else if (p.string_value == "film-diffusivity") {
                    // Store film diffusivity data
                    for (const auto &v : p.values) {
                        param.values.push_back(v);
                    }
                    for (const auto &type : p.particleTypes) {
                        param.particleTypes.insert(type);
                    }
                    param.particleTypes.insert("film-diffusivity");
                }
            }

            std::cout << "Parsed film-averaged with " << params.size() << " parameters" << std::endl;
        })] >> ')' >> ')';

// Rule for constant binary diffusivity
auto const constant_binary_diffusivity = x3::rule<class constant_binary_diffusivity_, Parameter>{
        "constant_binary_diffusivity"}
                                                 = '(' >> x3::lit("constant") >> '.' >> x3::double_[([](auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTCOEFF;
            param.values = {{x3::_attr(ctx)}};
            std::cout << "Parsed constant binary diffusivity: " << x3::_attr(ctx) << std::endl;
        })] >> ')';

// 定义化学式属性规则
auto const chemical_formula_property = x3::rule<chemical_formula_property_class, Property>{"chemical_formula_property"}
                                               = (x3::lit("chemical-formula") >> simple_parameter)[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            prop.name = "chemical-formula";
            prop.parameters.push_back(x3::_attr(ctx));
            std::cout << "Parsed chemical formula property with parameter: " << prop.parameters[0].string_value
                      << std::endl;
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

// Define a rule for parsing a list of particle types
auto const particle_types = x3::rule<class particle_types_, std::vector<std::string>>{}
                                    = +symbol;

// Define a rule for parsing fluid property with particle types
auto const fluid_property = x3::rule<class fluid_property_, Property>{"fluid_property"}
                                    = (x3::lit("fluid") >> -particle_types)[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            prop.name = "fluid";

            // Create a parameter to hold the particle types
            Parameter param;
            param.coeff = CONSTCOEFF;
            param.string_value = "particle_types"; // Marker to indicate this contains particle types

            // Store the particle types in the parameter
            auto types = x3::_attr(ctx);
            if (types) { // Check if particle types were provided
                for (const auto &type: *types) {
                    // For each type, add a value of -999.0 as a placeholder
                    param.values.push_back({-999.0});
                    // And store the actual type in the string_value
                    if (param.string_value.size() > 13) { // > "particle_types"
                        param.string_value += " ";
                    }
                    param.string_value += type;
                }
            }

            prop.parameters.push_back(param);
            std::cout << "Parsed fluid property with " << (types ? types->size() : 0) << " particle types" << std::endl;
        })];

// Define a rule for parsing a material type property enclosed in parentheses
auto const material_type_property = x3::rule<class material_type_property_, Property>{"material_type_property"}
                                            = '(' >> symbol[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            std::string type = x3::_attr(ctx);

            // Only handle known material types
            if (type == "solid" || type == "fluid" || type == "mixture") {
                prop.name = type;
                std::cout << "Parsed material type property: " << type << std::endl;
            } else {
                // If not a known material type, set to empty (will be ignored later)
                prop.name = "";
            }
        })] >> *symbol[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            if (!prop.name.empty()) {  // Only process particle types if material type is valid
                std::string particle_type = x3::_attr(ctx);

                // Create parameter to hold particle types
                if (prop.parameters.empty()) {
                    Parameter param;
                    param.coeff = CONSTCOEFF;
                    param.string_value = "particle_types";
                    prop.parameters.push_back(param);
                }

                // Add particle type
                auto &param = prop.parameters[0];
                param.values.push_back({-999.0});
                if (param.string_value.size() > 13) {  // > "particle_types"
                    param.string_value += " ";
                }
                param.string_value += particle_type;

                std::cout << "Added particle type: " << particle_type << " to " << prop.name << std::endl;
            }
        })] >> ')';

auto const binary_diffus_param = x3::rule<class binary_diffus_param_, Parameter>{"binary_diffus_param"}
                                         = (binDiff_param_symbols >>
                                                                  // 处理averaging-coefficient参数，它是一个简单值
                                                                  ((x3::lit("averaging-coefficient")
                                                                          >> simple_parameter)[([](auto &ctx) {
                                                                      auto &param = x3::_val(ctx);
                                                                      param.coeff = CONSTCOEFF;
                                                                      param.string_value = "averaging-coefficient";

                                                                      // 复制简单参数的值
                                                                      auto simple_param = x3::_attr(ctx);
                                                                      param.values = simple_param.values;

                                                                      std::cout
                                                                              << "Parsed averaging-coefficient parameter with value: "
                                                                              << param.values[0][0] << std::endl;
                                                                  })] |
                                                                   // 处理film-diffusivity参数，它可以包含嵌套的多项式和常量
                                                                   (x3::lit("film-diffusivity") >> '(' >>
                                                                                                *(parameter[([](
                                                                                                        auto &ctx) {
                                                                                                    auto &param = x3::_val(
                                                                                                            ctx);
                                                                                                    if (param.string_value.empty()) {
                                                                                                        param.string_value = "film-diffusivity";
                                                                                                        param.coeff = CONSTCOEFF;
                                                                                                        // 创建一个占位符值
                                                                                                        param.values.push_back(
                                                                                                                {-999.0});
                                                                                                    }
                                                                                                    // 将子参数存储在particleTypes集合中（临时使用此字段存储子参数信息）
                                                                                                    auto sub_param = x3::_attr(
                                                                                                            ctx);
                                                                                                    std::string param_info = std::to_string(
                                                                                                            static_cast<int>(sub_param.coeff));
                                                                                                    if (!sub_param.values.empty() &&
                                                                                                        !sub_param.values[0].empty()) {
                                                                                                        param_info +=
                                                                                                                ":" +
                                                                                                                std::to_string(
                                                                                                                        sub_param.values[0][0]);
                                                                                                    }
                                                                                                    param.particleTypes.insert(
                                                                                                            param_info);

                                                                                                    std::cout
                                                                                                            << "Added nested parameter to film-diffusivity"
                                                                                                            << std::endl;
                                                                                                })]) >> ')')[([](
                                                                           auto &ctx) {
                                                                       auto &param = x3::_val(ctx);
                                                                       param.string_value = "film-diffusivity";
                                                                       param.coeff = CONSTCOEFF;
                                                                       if (param.values.empty()) {
                                                                           // 创建一个占位符值
                                                                           param.values.push_back({-999.0});
                                                                       }
                                                                       std::cout
                                                                               << "Parsed film-diffusivity parameter with nested structure"
                                                                               << std::endl;
                                                                   })]));


auto const film_averaged_diffusivity = x3::rule<class film_averaged_diffusivity_, Parameter>{
        "film_averaged_diffusivity"}
                                               = (x3::lit("film-averaged") >> '(' >> *binary_diffus_param >> ')')[([](
                auto &ctx) {
            auto &param = x3::_val(ctx);
            param.coeff = CONSTCOEFF; // 使用常量类型
            param.string_value = "film-averaged";

            // 获取子参数列表
            auto sub_params = x3::_attr(ctx);

            // 创建一个嵌套的值结构来存储子参数
            std::vector<double> placeholder = {-999.0}; // 占位符
            param.values.push_back(placeholder);

            // 将子参数存储在particleTypes集合中（临时使用此字段存储子参数信息）
            for (const auto &sub_param: sub_params) {
                for (const auto &type: sub_param.particleTypes) {
                    param.particleTypes.insert(type);
                }
                if (sub_param.particleTypes.empty()) {
                    param.particleTypes.insert(sub_param.string_value);
                }
            }

            std::cout << "Parsed film-averaged diffusivity with " << sub_params.size() << " parameters" << std::endl;
        })];

// 定义binary-diffusivity属性规则
auto const binary_diffusivity_property = x3::rule<class binary_diffusivity_property_, Property>{"binary_diffusivity_property"}
    = '(' >> x3::lit("binary-diffusivity") >>
        (
            // Handle any combination of film_averaged_param and constant_binary_diffusivity
            *(film_averaged_param | constant_binary_diffusivity)
        )[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            prop.name = "binary-diffusivity";

            // Add all parameters
            auto params = x3::_attr(ctx);
            for (const auto &variant_param : params) {
                // Use boost::get to extract the Parameter from the variant
                const Parameter &p = boost::get<Parameter>(variant_param);
                prop.parameters.push_back(p);
            }

            std::cout << "Parsed binary-diffusivity with " << prop.parameters.size() << " parameters" << std::endl;
        })] >> ')';


// 定义属性规则
// Update the property rule to include the fluid_property
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
                | fluid_property[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    const Property &parsed = x3::_attr(ctx);
                    prop.name = parsed.name;
                    prop.parameters = parsed.parameters;
                    std::cout << "Parsed fluid property with " << prop.parameters.size() << " parameters" << std::endl;
                })]
                | material_type_property[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    const Property &parsed = x3::_attr(ctx);
                    if (!parsed.name.empty()) {
                        prop.name = parsed.name;
                        prop.parameters = parsed.parameters;
                        std::cout << "Parsed material type property with " << prop.parameters.size() << " parameters"
                                  << std::endl;
                    }
                })]
                | binary_diffusivity_property[([](auto &ctx) {
                    auto &prop = x3::_val(ctx);
                    const Property &parsed = x3::_attr(ctx);
                    prop.name = parsed.name;
                    prop.parameters = parsed.parameters;
                    std::cout << "Parsed binary-diffusivity property with " << prop.parameters.size() << " parameters"
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
// Define a rule for parsing material type with optional particle types
auto const material_type_with_particles = x3::rule<class material_type_with_particles_, std::pair<std::string, std::vector<std::string>>>{}
                                                  = symbol[([](auto &ctx) {
            auto &type_pair = x3::_val(ctx);
            type_pair.first = x3::_attr(ctx); // The material type (solid, fluid, etc.)
        })] >> *symbol[([](auto &ctx) {
            auto &type_pair = x3::_val(ctx);
            type_pair.second.push_back(x3::_attr(ctx)); // Add particle type
        })];


// 定义单独的材料类型属性规则
auto const standalone_material_type = x3::rule<class standalone_material_type_, Property>{"standalone_material_type"}
                                              = symbol[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            std::string type = x3::_attr(ctx);

            // 只处理已知的材料类型
            if (type == "solid" || type == "fluid" || type == "mixture") {
                prop.name = type;
                std::cout << "Parsed standalone material type: " << type << std::endl;
            } else {
                // 如果不是已知的材料类型，则设置为空，后续会被忽略
                prop.name = "";
            }
        })] >> *symbol[([](auto &ctx) {
            auto &prop = x3::_val(ctx);
            if (!prop.name.empty()) {  // 只有当材料类型有效时才处理粒子类型
                std::string particle_type = x3::_attr(ctx);

                // 创建参数来保存粒子类型
                if (prop.parameters.empty()) {
                    Parameter param;
                    param.coeff = CONSTCOEFF;
                    param.string_value = "particle_types";
                    prop.parameters.push_back(param);
                }

                // 添加粒子类型
                auto &param = prop.parameters[0];
                param.values.push_back({-999.0});
                if (param.string_value.size() > 13) {  // > "particle_types"
                    param.string_value += " ";
                }
                param.string_value += particle_type;

                std::cout << "Added particle type: " << particle_type << " to " << prop.name << std::endl;
            }
        })];

// 定义材料规则
// 修改材料规则
auto const material = x3::rule<material_class, MaterialData>{"material"}
                              = '(' >> symbol[([](auto &ctx) {
            auto &mat = x3::_val(ctx);
            mat.name = x3::_attr(ctx);
            std::cout << "Parsed material name: " << mat.name << std::endl;
        })] >>
                                    // 确保材料类型在材料名称之后立即解析
                                    (
                                            // 处理括号内的材料类型声明，如 (solid inert-particle)
                                            ('(' >> symbol[([](auto &ctx) {
                                                auto &mat = x3::_val(ctx);
                                                std::string type = x3::_attr(ctx);

                                                // 设置材料类型
                                                if (type == "solid" || type == "fluid" || type == "mixture") {
                                                    mat.type = type;
                                                    std::cout << "Parsed material type in parentheses: " << type
                                                              << std::endl;
                                                    // 创建属性
                                                    Property prop;
                                                    prop.name = type;
                                                    mat.properties.push_back(prop);
                                                }
                                            })] >> *symbol[([](auto &ctx) {
                                                auto &mat = x3::_val(ctx);
                                                std::string particle_type = x3::_attr(ctx);
                                                // 确保最后一个属性是材料类型属性
                                                if (!mat.properties.empty()) {
                                                    Property &prop = mat.properties.back();
                                                    if (prop.name == mat.type) {
                                                        // 创建参数来保存粒子类型
                                                        if (prop.parameters.empty()) {
                                                            Parameter param;
                                                            param.coeff = CONSTCOEFF;
                                                            param.string_value = "particle_types";
                                                            prop.parameters.push_back(param);
                                                        }

                                                        // 添加粒子类型
                                                        auto &param = prop.parameters[0];
                                                        param.values.push_back({-999.0});
                                                        if (param.string_value.size() > 13) { // > "particle_types"
                                                            param.string_value += " ";
                                                        }
                                                        param.particleTypes.emplace(particle_type);
                                                        std::cout << "Added particle type: " << particle_type << " to "
                                                                  << prop.name << std::endl;
                                                    }
                                                }
                                            })] >> ')')
                                            |
                                            // 处理直接的材料类型声明，如 solid inert-particle
                                            (symbol[([](auto &ctx) {
                                                auto &mat = x3::_val(ctx);
                                                std::string type = x3::_attr(ctx);

                                                // 设置材料类型
                                                if (type == "solid" || type == "fluid" || type == "mixture") {
                                                    mat.type = type;
                                                    std::cout << "Parsed direct material type: " << type << std::endl;

                                                    // 创建属性
                                                    Property prop;
                                                    prop.name = type;
                                                    mat.properties.push_back(prop);
                                                    std::cout << "Added standalone material type property: "
                                                              << prop.name << std::endl;
                                                }
                                            })] >> *symbol[([](auto &ctx) {
                                                auto &mat = x3::_val(ctx);
                                                std::string particle_type = x3::_attr(ctx);

                                                // 确保最后一个属性是材料类型属性
                                                if (!mat.properties.empty()) {
                                                    Property &prop = mat.properties.back();
                                                    if (prop.name == mat.type) {
                                                        // 创建参数来保存粒子类型
                                                        if (prop.parameters.empty()) {
                                                            Parameter param;
                                                            param.coeff = CONSTCOEFF;
                                                            param.string_value = "particle_types";
                                                            prop.parameters.push_back(param);
                                                        }

                                                        // 添加粒子类型
                                                        auto &param = prop.parameters[0];
                                                        param.values.push_back({-999.0});
                                                        if (param.string_value.size() > 13) { // > "particle_types"
                                                            param.string_value += " ";
                                                        }
                                                        param.string_value += particle_type;

                                                        std::cout << "Added particle type: " << particle_type << " to "
                                                                  << prop.name << std::endl;
                                                    }
                                                }
                                            })])
                                    ) >> *(
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
        if (propertyTypeNames.count(key) > 0) {

            for (const auto &param: prop.parameters) {
                MaterialProperty mp;
                mp.name = key;
                mp.unit = "";
                mp.coeffType = param.coeff;
                switch (param.coeff) {
                    case coefficientType::CONSTCOEFF: {

                        mp.constData = param.values[0][0];
                        std::cout << "Set property: " << key << " = " << param.values[0][0] << std::endl;
                        break;
                    }
                    case polynomialTPieceLinearT:
                        // Handle linear case if needed, e.g., by storing the slope and intercept
                    {
                        polyPiecewiseLinearData piecewiseData;
                        piecewiseData.temp_ranges.resize(param.values.size(), 0.0);
                        piecewiseData.coefficients.resize(param.values.size(), 0.0);
                        // For piecewise-linear, we expect two vectors: temperatures and values
                        for (int i = 0; i < param.values.size(); i++) {
                            piecewiseData.temp_ranges[i] = param.values[i][0];
                            piecewiseData.coefficients[i] = param.values[i][1];
                        }
                        mp.ppldata = piecewiseData;
                        std::cout << "Created piecewise polynomial data with "
                                  << piecewiseData.temp_ranges.size() << " temperature points" << std::endl;

                        break;
                    }
                    case polynomialTPiecePolyT:
                        // 使用 MaterialProperty 结构
                    {
                        PiecewisePolynomialData piecewiseData;
                        // For piecewise-linear, we expect two vectors: temperatures and values
                        if (param.values.size() == 2) {
                            // First vector contains temperatures, second contains values
                            piecewiseData.temp_ranges = param.values[0];
                            // Create coefficients for each temperature point (just the value as a constant)
                            for (size_t i = 0; i < param.values[1].size(); ++i) {
                                piecewiseData.coefficients.push_back({param.values[1][i]});
                            }
                            mp.pwpolydata = piecewiseData;
                            std::cout << "Created piecewise polynomial data with "
                                      << piecewiseData.temp_ranges.size() << " temperature points" << std::endl;
                        }
                        break;
                    }

                    case nasa9PiecePolyT:
                        // 使用 MaterialProperty 结构
                    {
                        NASAPolynomialData nasaData;
                        if (param.values.size() == 2) {

                            std::cout << "Created piecewise polynomial data with "
                                      << " temperature points" << std::endl;
                        }
                        mp.nasapolydata = nasaData;
                        break;
                    }
                    case polynomialT: {
                        PolynomialData dat;
                        dat.coefficients = param.values[0];
                        mp.polydata = dat;
                        break;
                    }
                    case compressibleT: {
                        PolynomialData dat;
                        dat.coefficients = param.values[0];
                        mp.polydata = dat;
                        break;
                    }
                    case sutherlandT: {
                        PolynomialData dat;
                        dat.coefficients = param.values[0];
                        mp.polydata = dat;
                        break;
                    }
                    case powerLawT: {
                        PolynomialData dat;
                        dat.coefficients = param.values[0];
                        mp.polydata = dat;
                        break;
                    }
                    case blottnerT: {
                        PolynomialData dat;
                        dat.coefficients = param.values[0];
                        mp.polydata = dat;
                        break;
                    }
                    default:
                        std::cout << "Unsupported coefficient type: " << param.coeff << std::endl;
                }
                material.properties[key].push_back(mp);
            }

        } else {
            if (key == "fluid") {
                // Ensure the material state is set to FLUID
                material.type.state = MaterialState::FLUID;

                if (!prop.parameters.empty()) {
                    const auto &param = prop.parameters[0];
                    if (param.string_value.find("particle_types") == 0) {
                        // Extract particle types from the string_value
                        std::string types_str = param.string_value.substr(14); // Skip "particle_types "
                        std::istringstream iss(types_str);
                        std::vector<std::string> particle_types;
                        std::string type;
                        while (iss >> type) {
                            particle_types.push_back(type);
                        }
                        // Create a MaterialProperty for the particle types
                        MaterialProperty mp;
                        mp.name = "fluid";
                        mp.unit = "";

                        material.properties["fluid"].push_back(mp);

                        // Set the particle flags in the material's type
                        std::cout << "Added " << particle_types.size() << " particle types to material" << std::endl;
                    }
                } else {
                    // If there are no parameters, just set the material state to FLUID
                    MaterialProperty mp;
                    mp.name = "fluid";
                    mp.unit = "";
                    material.properties["fluid"].push_back(mp);
                }
                continue;
            }
            // 处理binary-diffusivity属性
            if (key == "binary-diffusivity") {
                for (const auto &param: prop.parameters) {
                    if (param.string_value == "film-averaged") {
                        // 处理film-averaged类型的binary-diffusivity
                        // 创建一个MaterialProperty来存储film-averaged diffusivity数据
                        MaterialProperty mp;
                        mp.name = "binary-diffusivity";
                        mp.unit = "";
                        //mp.coeffType = FILM_AVERAGED_DIFFUSION;

                        // 查找averaging-coefficient和film-diffusivity子参数
                        double averaging_coeff = 0.3333; // 默认值
                        MaterialProperty film_diff_prop;
                        film_diff_prop.name = "film-diffusivity";
                        film_diff_prop.unit = "";
                        film_diff_prop.coeffType = CONSTCOEFF;
                        film_diff_prop.constData = 0.0;

                        // 从particleTypes集合中提取子参数信息
                        for (const auto &info: param.particleTypes) {
                            if (info.find("averaging-coefficient") != std::string::npos) {
                                // 提取averaging-coefficient值
                                for (const auto &sub_param: prop.parameters) {
                                    if (sub_param.string_value == "averaging-coefficient") {
                                        if (!sub_param.values.empty() && !sub_param.values[0].empty()) {
                                            averaging_coeff = sub_param.values[0][0];
                                        }
                                        break;
                                    }
                                }
                            } else if (info.find("film-diffusivity") != std::string::npos) {
                                // 提取film-diffusivity参数
                                for (const auto &sub_param: prop.parameters) {
                                    if (sub_param.string_value == "film-diffusivity") {
                                        // 处理film-diffusivity的子参数
                                        for (const auto &sub_info: sub_param.particleTypes) {
                                            size_t colon_pos = sub_info.find(':');
                                            if (colon_pos != std::string::npos) {
                                                int coeff_type = std::stoi(sub_info.substr(0, colon_pos));
                                                double value = std::stod(sub_info.substr(colon_pos + 1));

                                                if (coeff_type == CONSTCOEFF) {
                                                    film_diff_prop.coeffType = CONSTCOEFF;
                                                    film_diff_prop.constData = value;
                                                } else if (coeff_type == polynomialTPieceLinearT) {
                                                    film_diff_prop.coeffType = polynomialTPieceLinearT;
                                                    // 这里需要更复杂的处理来提取piecewise-linear数据
                                                    // 但由于我们没有完整的数据结构，这里简化处理
                                                }
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }

                        // 存储film-averaged diffusivity数据
                        // 由于我们没有直接的FilmAveragedDiffusivityData字段，
                        // 我们将数据存储在MaterialProperty的constData和其他字段中
                        mp.constData = averaging_coeff; // 存储averaging_coefficient

                        // 将film-diffusivity作为一个单独的MaterialProperty添加到properties中
                        material.properties["binary-diffusivity"].push_back(mp);
                        material.properties["film-diffusivity"].push_back(film_diff_prop);

                        std::cout << "Processed film-averaged binary-diffusivity with averaging coefficient: "
                                  << averaging_coeff << std::endl;
                    } else {
                        // 处理常规binary-diffusivity参数
                        MaterialProperty mp;
                        mp.name = "binary-diffusivity";
                        mp.unit = "";
                        mp.coeffType = param.coeff;

                        if (!param.values.empty() && !param.values[0].empty()) {
                            mp.constData = param.values[0][0];
                        }

                        material.properties["binary-diffusivity"].push_back(mp);
                        std::cout << "Processed regular binary-diffusivity parameter" << std::endl;
                    }
                }
                continue;
            }
            // 处理化学式
            if (key == "chemical-formula" && !prop.parameters.empty()) {
                const auto &param = prop.parameters[0];
                if (!param.string_value.empty() && (param.string_value != "#f") && (param.string_value != "#t")) {
                    // 设置 chemical_formula 字段
                    material.chemical_formula = param.string_value;
                    std::cout << "Set chemical formula to: " << param.string_value << std::endl;
                }
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
                    MaterialProperty mp;
                    mp.name = "species";
                    mp.unit = "";

                    material.properties["species"].push_back(mp);

                    // Also store the species names in the material's species field
                    material.speciesName = species_names;
                }
                continue;
            }
        }

    }
}


// 定义binary-diffusivity参数规则


// 定义film-averaged类型的binary-diffusivity规则


