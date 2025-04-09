#include "scm_parser.h"
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>
#include <algorithm>
#include <fstream>

// 适配PropertyPair结构用于Boost.Fusion
BOOST_FUSION_ADAPT_STRUCT(
        CFD_MaterialDB::PropertyPair,
        name,
        value
)

// 适配MaterialData结构用于Boost.Fusion
BOOST_FUSION_ADAPT_STRUCT(
        CFD_MaterialDB::MaterialData,
        name,
        type,
        properties
)
namespace CFD_MaterialDB {
    namespace x3 = boost::spirit::x3;

    // 定义解析规则
    auto const identifier = x3::lexeme[+(x3::alnum | x3::char_("-") | x3::char_("_") | x3::char_("<") |
                                         x3::char_(">"))];
    auto const quoted_string = x3::lexeme['"' >> *(x3::char_ - '"') >> '"'];

    // 前置声明
    x3::rule<class property_value_class, PropertyValue> const property_value = "property_value";
    x3::rule<class property_pair_class, PropertyPair> const property_pair = "property_pair";
    x3::rule<class property_list_class, std::vector<PropertyPair>> const property_list = "property_list";
    x3::rule<class material_class, MaterialData> const material = "material";
    x3::rule<class materials_class, std::vector<MaterialData>> const materials = "materials";
    x3::rule<class nested_properties_class, std::vector<PropertyPair>> const nested_properties = "nested_properties";

    // ... existing code ...

// 修改所有语义动作函数，确保它们返回 true
    auto set_string_value = [](auto &ctx) -> bool {
        _val(ctx).string_value = _attr(ctx);
        _val(ctx).is_numeric = false;
        _val(ctx).is_none = false;
        _val(ctx).has_nested = false;
        return true;  // 显式返回 true
    };

    auto set_numeric_value = [](auto &ctx) -> bool {
        _val(ctx).numeric_value = _attr(ctx);
        _val(ctx).is_numeric = true;
        _val(ctx).is_none = false;
        _val(ctx).has_nested = false;
        return true;  // 显式返回 true
    };

    auto set_bool_value = [](auto &ctx) -> bool {
        // The attribute is already captured as 't' or 'f'
        char bool_char = _attr(ctx);
        _val(ctx).string_value = (bool_char == 't') ? "true" : "false";
        _val(ctx).is_numeric = false;
        _val(ctx).is_none = false;
        _val(ctx).has_nested = false;
        return true;  // 显式返回 true
    };


    auto set_none_value = [](auto &ctx) -> bool {
        _val(ctx).is_none = true;
        _val(ctx).is_numeric = false;
        _val(ctx).has_nested = false;
        return true;  // 显式返回 true
    };

// 嵌套属性解析定义
    auto const nested_properties_def =
            *('(' >> property_pair >> ')');

    struct process_nested_properties {
        template<typename Context>
        bool operator()(Context &ctx) const {
            // 创建一个新的shared_ptr<vector>并为其填充数据
            auto nested = std::make_shared<std::vector<PropertyPair>>();
            for (const auto &prop: _attr(ctx)) {
                nested->push_back(prop);
            }
            _val(ctx).nested_properties = nested;
            _val(ctx).has_nested = true;
            _val(ctx).is_numeric = false;
            _val(ctx).is_none = false;
            return true;  // 显式返回 true
        }
    };

    // And modify the parser rule:
    auto const property_value_def =
            quoted_string[set_string_value] |
            x3::double_[set_numeric_value] |
            x3::int_[set_numeric_value] |
            (x3::lit("#") >> x3::char_("tf"))[set_bool_value] |  // Changed to char_("tf")
            x3::lit("#f")[set_none_value] |
            ('(' >> nested_properties >> ')')[process_nested_properties()];
    // 属性对解析
    auto const property_pair_def =
            (identifier >> "." >> property_value) | // 处理 (property . value) 格式
            (identifier >> property_value);        // 处理 (property value) 格式

    // 属性列表解析
    auto const property_list_def =
            *('(' >> property_pair >> ')');

    // 材料解析
    auto const material_def =
            '(' >> identifier >> identifier >> property_list >> ')';

    // 材料列表解析
    auto const materials_def =
            '(' >> *material >> ')';

    BOOST_SPIRIT_DEFINE(property_value, property_pair, property_list, material, materials, nested_properties);


    std::vector<Material> ScmParser::parse(const std::string &filename) {
        std::vector<Material> materials;

        // 读取文件内容
        std::ifstream file(filename);
        if (!file.is_open()) {
            return materials;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // 解析内容
        std::vector<MaterialData> parsed_materials;
        auto iter = content.begin();
        auto end = content.end();

        try {
            bool success = x3::phrase_parse(iter, end,
                                            CFD_MaterialDB::materials,
                                            x3::space,
                                            parsed_materials);

            if (!success || iter != end) {
                std::cerr << "解析失败，剩余未解析内容: " << std::string(iter, std::min(iter + 100, end)) << "..."
                          << std::endl;
                return materials;
            }

            // 转换解析结果到Material对象
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
                } else if (typeStr == "COMBUSTING-PARTICLE" || typeStr == "COMBUSTION") {
                    material.type.state = MaterialState::SOLID;
                    material.type.addParticleType(ParticleType::COMBUSTING_PARTICLE);
                } else if (typeStr == "INERT-PARTICLE") {
                    material.type.state = MaterialState::SOLID;
                    material.type.addParticleType(ParticleType::INERT_PARTICLE);
                } else if (typeStr == "DROPLET-PARTICLE") {
                    material.type.state = MaterialState::SOLID;
                    material.type.addParticleType(ParticleType::DROPLET_PARTICLE);
                }

                // 处理属性
                processProperties(material, mat_data.properties);

                materials.push_back(material);
            }
        } catch (const std::exception &e) {
            std::cerr << "解析异常: " << e.what() << std::endl;
        }

        return materials;
    }


    void ScmParser::processSpeciesProperty(Material &material,
                                           const std::shared_ptr<std::vector<PropertyPair>> &species_props) {
        for (const auto &prop_pair: *species_props) {
            if (prop_pair.name == "names" && prop_pair.value.has_nested) {
                // 提取species名称列表
                std::vector<std::string> species_names;
                for (const auto &nested_pair: *(prop_pair.value.nested_properties)) {
                    if (!nested_pair.value.is_none) {
                        species_names.push_back(nested_pair.name);
                    }
                }

                // 保存到material中
                nlohmann::json species_json;
                species_json["names"] = species_names;
                material.properties["species"] = {
                        "species", "", species_json.dump(), "object"
                };
            }
        }
    }

// 处理reactions属性
    void ScmParser::processReactionsProperty(Material &material,
                                             const std::shared_ptr<std::vector<PropertyPair>> &reactions_props) {
        for (const auto &prop_pair: *reactions_props) {
            if (prop_pair.name == "finite-rate" && prop_pair.value.has_nested) {
                // 解析有限速率反应
                nlohmann::json reactions_json;
                std::vector<nlohmann::json> reaction_list;

                // 遍历每个反应
                for (const auto &reaction_pair: *(prop_pair.value.nested_properties)) {
                    nlohmann::json reaction;
                    reaction["name"] = reaction_pair.name;

                    // 解析反应详情
                    if (reaction_pair.value.has_nested) {
                        for (const auto &detail: *(reaction_pair.value.nested_properties)) {
                            // 根据类型处理反应的各种参数
                            if (detail.value.is_numeric) {
                                reaction[detail.name] = detail.value.numeric_value;
                            } else if (detail.value.has_nested) {
                                // 处理复杂的嵌套结构，如stoichiometry, arrhenius等
                                reaction[detail.name] = nlohmann::json::parse(
                                        processNestedProperties(detail.value.nested_properties));
                            } else if (detail.value.is_none) {
                                reaction[detail.name] = nullptr;
                            } else {
                                reaction[detail.name] = detail.value.string_value;
                            }
                        }
                    }
                    reaction_list.push_back(reaction);
                }

                reactions_json["finite-rate"] = reaction_list;
                material.properties["reactions"] = {
                        "reactions", "", reactions_json.dump(), "object"
                };
            }
        }
    }

    // 处理属性，包括嵌套属性
    void ScmParser::processProperties(Material &material, const std::vector<PropertyPair> &properties) {
        for (const auto &prop_pair: properties) {
            const auto &key = prop_pair.name;
            const auto &prop = prop_pair.value;

            if (key == "species" && prop.has_nested) {
                // 特殊处理species属性
                processSpeciesProperty(material, prop.nested_properties);
            } else if (key == "reactions" && prop.has_nested) {
                // 特殊处理reactions属性
                processReactionsProperty(material, prop.nested_properties);
            } else if (prop.is_none) {
                // 处理不存在的值（#f表示none）
                material.properties[key] = {
                        key, "", nullptr, "null"
                };
            } else if (prop.has_nested) {
                // 处理一般嵌套属性
                std::string nestedJson = processNestedProperties(prop.nested_properties);
                material.properties[key] = {
                        key, "", nestedJson, "object"
                };
            } else if (prop.is_numeric) {
                material.properties[key] = {
                        key, "", prop.numeric_value, "number"
                };
            } else {
                material.properties[key] = {
                        key, "", prop.string_value, "string"
                };
            }
        }
    }

    // 处理嵌套属性并转换为JSON字符串 - 接受shared_ptr参数的版本
    std::string ScmParser::processNestedProperties(const std::shared_ptr<std::vector<PropertyPair>> &nested_props) {
        nlohmann::json j;

        for (const auto &prop_pair: *nested_props) {
            const auto &nested_key = prop_pair.name;
            const auto &nested_prop = prop_pair.value;

            if (nested_prop.is_none) {
                // 处理不存在的值（#f表示none）
                j[nested_key] = nullptr;
            } else if (nested_prop.has_nested) {
                // 递归处理更深层次的嵌套
                // 使用正确的函数重载，确保传递shared_ptr类型参数
                j[nested_key] = nlohmann::json::parse(processNestedProperties(nested_prop.nested_properties));
            } else if (nested_prop.is_numeric) {
                j[nested_key] = nested_prop.numeric_value;
            } else {
                j[nested_key] = nested_prop.string_value;
            }
        }

        return j.dump();
    }

    // 处理嵌套属性并转换为JSON字符串 - 接受vector<pair>参数的版本
    std::string
    ScmParser::processNestedProperties(const std::vector<std::pair<std::string, PropertyValue>> &nested_props) {
        nlohmann::json j;

        for (const auto &[nested_key, nested_prop]: nested_props) {
            if (nested_prop.is_none) {
                // 处理不存在的值（#f表示none）
                j[nested_key] = nullptr;
            } else if (nested_prop.has_nested && nested_prop.nested_properties) {
                // 递归处理更深层次的嵌套
                // 显式调用接受shared_ptr参数的重载版本，确保类型匹配
                const std::shared_ptr<std::vector<PropertyPair>> &nested = nested_prop.nested_properties;
                j[nested_key] = nlohmann::json::parse(processNestedProperties(nested));
            } else if (nested_prop.is_numeric) {
                j[nested_key] = nested_prop.numeric_value;
            } else {
                j[nested_key] = nested_prop.string_value;
            }
        }

        return j.dump();
    }
}









       