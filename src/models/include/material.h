#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <array>
#include <nlohmann/json.hpp>

#include <optional>
    enum class MaterialState {
        FLUID,
        SOLID,
        MIXTURE
    };

    enum class ParticleType {
        NONE = 0,
        INERT_PARTICLE = 1 << 0,
        DROPLET_PARTICLE = 1 << 1,
        COMBUSTING_PARTICLE = 1 << 2
    };

    struct MaterialType {
        MaterialState state;
        int particle_flags;

        MaterialType() = default;

        explicit MaterialType(int flags) : particle_flags(flags) {}

        bool hasParticleType(ParticleType type) const {
            return particle_flags & static_cast<int>(type);
        }

        void addParticleType(ParticleType type) {
            particle_flags |= static_cast<int>(type);
        }

        void removeParticleType(ParticleType type) {
            particle_flags &= ~static_cast<int>(type);
        }
    };

    struct NASAPolynomialData {
        std::array<std::vector<double>, 3> segments;
        std::array<double, 2> temp_ranges;
    };

    struct PiecewisePolynomialData {
        std::vector<std::vector<double>> coefficients;
        std::vector<double> temp_ranges;
    };

    struct PolynomialData {
        std::vector<double> coefficients;
        double min_temp;
        double max_temp;
    };

    struct MaterialProperty {
        std::string name;
        std::string unit;
        std::variant<double, std::vector<double>, std::string, PolynomialData, NASAPolynomialData, PiecewisePolynomialData> data;
        std::string type;
    };


    class Material {
    public:
        Material() = default;

        // 从SCM字符串创建Material对象
//        static Material fromScmString(const std::string &scmContent) {
//            return Material(scmContent);
//        }

        // 从SQLite数据库行创建Material对象
        static Material fromSqlite(const std::string &dbRow);

        // 获取JSON字符串
        std::string toJson() const {
            nlohmann::json j;
            j["name"] = name;
            j["type"] = static_cast<int>(type.state);
            j["description"] = description;

            nlohmann::json propsJson;
            for (const auto &[key, prop]: properties) {
                nlohmann::json propJson;
                propJson["name"] = prop.name;
                propJson["unit"] = prop.unit;
                propJson["type"] = prop.type;

                std::visit([&](auto &&arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::vector<double>>) {
                        propJson["data"] = arg;
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        propJson["data"] = arg;
                    } else if constexpr (std::is_same_v<T, PolynomialData>) {
                        nlohmann::json polyJson;
                        polyJson["coefficients"] = arg.coefficients;
                        polyJson["min_temp"] = arg.min_temp;
                        polyJson["max_temp"] = arg.max_temp;
                        propJson["data"] = polyJson;
                    } else if constexpr (std::is_same_v<T, NASAPolynomialData>) {
                        nlohmann::json nasaJson;
                        nasaJson["segments"] = arg.segments;
                        nasaJson["temp_ranges"] = arg.temp_ranges;
                        propJson["data"] = nasaJson;
                    } else if constexpr (std::is_same_v<T, PiecewisePolynomialData>) {
                        nlohmann::json piecewiseJson;
                        piecewiseJson["coefficients"] = arg.coefficients;
                        piecewiseJson["temp_ranges"] = arg.temp_ranges;
                        propJson["data"] = piecewiseJson;
                    }
                }, prop.data);

                propsJson[key] = propJson;
            }

            // 添加对混合物组分和反应式的特殊处理
            if (properties.count("species")) {
                std::visit([&](auto &&arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        j["species"] = arg;
                    }
                }, properties.at("species").data);
            }
            if (properties.count("reactions")) {
                std::visit([&](auto &&arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        j["reactions"] = arg;
                    }
                }, properties.at("reactions").data);
            }

            j["properties"] = propsJson;

            return j.dump();
        }

        // 从JSON字符串解析
        void fromJson(const std::string &jsonStr) {
            nlohmann::json j = nlohmann::json::parse(jsonStr);
            name = j["name"].get<std::string>();
            type.state = static_cast<MaterialState>(j["type"].get<int>());
            description = j["description"].get<std::string>();

            if (j.contains("properties")) {
                nlohmann::json propsJson = j["properties"];
                for (auto &[key, propJson]: propsJson.items()) {
                    MaterialProperty prop;
                    prop.name = propJson["name"].get<std::string>();
                    prop.unit = propJson["unit"].get<std::string>();
                    prop.type = propJson["type"].get<std::string>();

                    if (prop.type == "number") {
                        prop.data = propJson["data"].get<double>();
                    } else if (prop.type == "string") {
                        prop.data = propJson["data"].get<std::string>();
                    } else if (prop.type == "polynomial") {
                        PolynomialData poly;
                        poly.coefficients = propJson["data"]["coefficients"].get<std::vector<double>>();
                        poly.min_temp = propJson["data"]["min_temp"].get<double>();
                        poly.max_temp = propJson["data"]["max_temp"].get<double>();
                        prop.data = poly;
                    } else if (prop.type == "nasa_polynomial") {
                        NASAPolynomialData nasa;
                        nasa.segments = propJson["data"]["segments"].get<std::array<std::vector<double>, 3>>();
                        nasa.temp_ranges = propJson["data"]["temp_ranges"].get<std::array<double, 2>>();
                        prop.data = nasa;
                    } else if (prop.type == "piecewise_polynomial") {
                        PiecewisePolynomialData piecewise;
                        piecewise.coefficients = propJson["data"]["coefficients"].get<std::vector<std::vector<double>>>();
                        piecewise.temp_ranges = propJson["data"]["temp_ranges"].get<std::vector<double>>();
                        prop.data = piecewise;
                    }
                    properties[key] = prop;
                }
            }

            if (j.contains("species")) {
                properties["species"] = {"species", "", j["species"].get<std::string>(), "string"};
            }
            if (j.contains("reactions")) {
                properties["reactions"] = {"reactions", "", j["reactions"].get<std::string>(), "string"};
            }
        }

        // 解析SCM格式的热力学数据
        void parseScmThermoData(const std::string &thermoBlock);

        // 获取属性
        const MaterialProperty &getProperty(const std::string &key) const {
            return properties.at(key);
        }

        // 检查属性是否存在
        bool hasProperty(const std::string &key) const {
            return properties.count(key) > 0;
        }

        // 成员字段
        std::string name;
        MaterialType type;
        std::string description;
        std::optional<std::string> chemical_formula;
        std::unordered_map<std::string, MaterialProperty> properties;

    private:


        void parseScmTransportData(const std::string &transportBlock);
    };

