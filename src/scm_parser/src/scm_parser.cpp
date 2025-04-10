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
// 适配PropertyPair结构用于Boost.Fusion
namespace x3 = boost::spirit::x3;

// Define coefficient_type_symbols (assuming it's defined elsewhere or provide a placeholder)
// Placeholder definition - replace with your actual symbol table/rule
x3::symbols<coefficientType> coefficient_type_symbols;

void init_symbols() {
    // Add symbols like "constant", "polynomialT", etc.
    coefficient_type_symbols.add
            ("constant", CONSTANT)
            ("polynomialT", polynomialT)
            ("polynomialTPieceLinearT", polynomialTPieceLinearT)
            ("polynomialTPiecePolyT", polynomialTPiecePolyT)
            ("nasa9PiecePolyT", nasa9PiecePolyT);
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

// Adapt structs needed for parsing
BOOST_FUSION_ADAPT_STRUCT(
        MaterialData,
        name,               // std::string
        type,               // std::string
        properties          // std::vector<Property>
)

// --- Rule Definitions ---

// Define basic rules first
auto const symbol = x3::rule<struct symbol_, std::string>{"symbol"}
                            = x3::lexeme[+(x3::alnum | x3::char_("-<>=+_."))];
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

// Parameter rule (remains the same)
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

// --- Refactored Property Rules ---

// 1. Rule for the specific chemical-formula case
// This rule synthesizes a Property directly.
auto const chemical_formula_property_action = [](auto& ctx) {
    // The attribute comes from the 'symbol' rule matched just before this action
    std::string formula_str = x3::_attr(ctx);

    // The value to synthesize is the Property object for this rule
    Property prop;
    prop.name = "chemical-formula";

    Parameter param;
    param.coeff = CONSTANT;
    param.values = {{-999.0}}; // Marker
    param.string_value = formula_str; // Assign the captured formula string

    prop.parameters.push_back(param);

    x3::_val(ctx) = prop; // Assign the constructed Property as the synthesized value
};

auto const chemical_formula_property = x3::rule<class cfp_, Property>{"chemical_formula_property"}
    = (x3::lit("chemical-formula") >> '.' >> symbol)[chemical_formula_property_action]; // Action attached here

// 2. Rule for the general property case (name >> *parameters)
// Relies on BOOST_FUSION_ADAPT_STRUCT(Property, name, parameters)
auto const general_property = x3::rule<class gp_, Property>{"general_property"}
    = symbol >> *parameter; // Automatically maps symbol to name, *parameter to parameters vector

// 3. Combine the two property formats into the main property rule
auto const property = x3::rule<struct property_, Property>{"property"}
    = '(' >> (chemical_formula_property | general_property) >> ')'; // Use alternative operator

// --- Remaining Rules ---

// Material rule (depends on symbol and property)
auto const material = x3::rule<struct material_, MaterialData>{"material"}
                              = '('
                >> symbol       // name
                >> symbol       // type
                >> *property    // properties
                >> ')';

// Comment rule
auto const comment = x3::lexeme[';' >> *(x3::char_ - x3::eol) >> x3::eol];

// Top-level file rule (depends on material and comment)
auto const scm_file = x3::rule<struct scm_file_, std::vector<MaterialData>>{"scm_file"}
                              = x3::skip(comment | x3::space)[*material];

// --- ScmParser::parse method and processProperties remain the same ---
// ... (ScmParser::parse implementation) ...
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
        // Use the globally defined rules here
        bool success = x3::phrase_parse(iter, end, scm_file, x3::space, parsed_materials);
        if (!success || iter != end) {
            std::cerr << "解析失败 at: '" << std::string(iter, std::min(iter + 20, end)) << "...'"
                      << std::endl;
            return materials_out;
        }

        // Conversion loop
        for (const auto &mat_data: parsed_materials) {
            Material material;
            material.name = mat_data.name;
            std::string typeStr = mat_data.type;
            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);

            // Set MaterialState based on type string
            if (typeStr == "FLUID") {
                material.type.state = MaterialState::FLUID;
            } else if (typeStr == "SOLID") {
                material.type.state = MaterialState::SOLID;
            } else if (typeStr == "MIXTURE") {
                material.type.state = MaterialState::MIXTURE;
            } // Add other type checks as needed

            processProperties(material, mat_data);
            materials_out.push_back(material);
        }
    }
    catch (const x3::expectation_failure<std::string::iterator> &e) {
        std::cerr << "Parse expectation failure near '"
                  << std::string(e.where(), std::min(e.where() + 20, content.end())) << "': Expected "
                  << e.which() << std::endl;
    }
    catch (const std::exception &e) {
        std::cerr << "解析异常: " << e.what() << std::endl;
    }

    return materials_out;
}

// ... (processProperties implementation) ...
void ScmParser::processProperties(Material &material, const MaterialData &mat_data) {
    for (const auto &prop: mat_data.properties) {
        const auto &key = prop.name;

        // The chemical formula case should now be handled directly by the parsed Property object
        // So, the special check here might need adjustment or removal depending on how you structure Material.properties

        // Example processing based on the Property object structure:
        if (key == "chemical-formula" && !prop.parameters.empty()) {
            const auto &param = prop.parameters[0];
            // Check if it was stored as a string using the special marker
            if (param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1 &&
                param.values[0][0] == -999.0 && !param.string_value.empty()) {
                material.properties["chemical-formula"] = { // Assuming Material::properties maps string to PropertyValue-like struct
                        "chemical-formula", "", param.string_value, "string"
                };
            }
             continue; // Processed chemical formula
        }

        // Handle general properties based on parameters
        if (!prop.parameters.empty()) {
            const auto &param = prop.parameters[0]; // Still assuming one parameter for simplicity
            bool processed = false;
            // Boolean check
            if (param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1 && (param.values[0][0] == 1.0 || param.values[0][0] == 0.0) && !param.string_value.empty() && (param.string_value=="#t" || param.string_value=="#f")) {
                 material.properties[key] = { key, "", (param.values[0][0] == 1.0), "boolean" };
                 processed = true;
            }
            // String check
            else if (param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1 && param.values[0][0] == -999.0 && !param.string_value.empty()) {
                 material.properties[key] = { key, "", param.string_value, "string" };
                 processed = true;
            }
            // Number check
            else if (!processed && param.coeff == CONSTANT && param.values.size() == 1 && param.values[0].size() == 1) {
                 material.properties[key] = { key, "", param.values[0][0], "number" };
                 processed = true; // Mark as processed
            }
            // Add checks for other coeff types (polynomialT, etc.) here
            // else if (param.coeff == polynomialT) { ... processed = true; }

        } else {
            // Handle properties with no parameters (flags)
            material.properties[key] = { key, "", true, "boolean" };
        }
    }
}