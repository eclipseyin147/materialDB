#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <variant>
#include <array>
#include <nlohmann/json.hpp>
#include <sstream>  // For std::istringstream
#include <optional>

enum MaterialState {
    INVALID = -1,
    FLUID,
    SOLID,
    MIXTURE
};

NLOHMANN_JSON_SERIALIZE_ENUM(MaterialState,
                             {
                                 { INVALID, nullptr },
                                 { FLUID, "fluid" },
                                 { SOLID, "solid" },
                                 { MIXTURE, "mixture" }
                             })

enum ParticleType {
    NONE = 0,
    INERT_PARTICLE = 1 << 0,
    DROPLET_PARTICLE = 1 << 1,
    COMBUSTING_PARTICLE = 1 << 2
};

NLOHMANN_JSON_SERIALIZE_ENUM(ParticleType,
                             {
                                 { NONE, nullptr },
                                 { INERT_PARTICLE, "inertParticle" },
                                 { DROPLET_PARTICLE, "dropletParticle" },
                                 { COMBUSTING_PARTICLE, "combustionParticle" }
                             })

struct MaterialType {
    MaterialType() = default;

    MaterialState state;
    std::unordered_set<ParticleType> particle_flags;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MaterialType, state, particle_flags)


struct NASAPolynomialData {
    std::array<std::vector<double>, 3> segments;
    std::array<double, 2> temp_ranges;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NASAPolynomialData, segments, temp_ranges)

struct polyPiecewiseLinearData {
    std::vector<double> temp_ranges;
    std::vector<double> coefficients;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(polyPiecewiseLinearData, temp_ranges, coefficients)


struct PiecewisePolynomialData {
    std::vector<std::vector<double>> coefficients;
    std::vector<double> temp_ranges;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PiecewisePolynomialData, coefficients, temp_ranges)

struct PolynomialData {
    std::vector<double> coefficients;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PolynomialData, coefficients)



enum coefficientType {
    NONET = 0,
    CONSTCOEFF,
    compressibleT,
    sutherlandT,
    powerLawT,
    blottnerT,
    polynomialT,
    polynomialTPieceLinearT,                 ///< std::vector<double>
    polynomialTPiecePolyT,   ///< std::vector<std::array<double, 7>>
    nasa9PiecePolyT   ///< std::vector<std::array<double, 9>>
};

NLOHMANN_JSON_SERIALIZE_ENUM(coefficientType,
                             {
                                 { NONET, nullptr },
                                 { CONSTCOEFF, "constant" },
                                 { compressibleT, "compressible" },
                                 { sutherlandT, "sutherland" },
                                 { powerLawT, "power-law" },
                                 { blottnerT, "blottner" },
                                 { polynomialT, "polynomial" },
                                 { polynomialTPieceLinearT, "polynomial piecewise-linear" },
                                 { polynomialTPiecePolyT, "polynomial piecewise-polynomial" },
                                 { nasa9PiecePolyT, "polynomial nasa-9-piecewise-polynomial" }
                                 })


struct MaterialProperty {
    std::string name;
    coefficientType coeffType;
    std::string unit;
    double constData = 0.0;
    PolynomialData polydata;
    polyPiecewiseLinearData ppldata;
    NASAPolynomialData nasapolydata;
    PiecewisePolynomialData pwpolydata;


};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MaterialProperty, name, coeffType, unit, constData, polydata, ppldata,
                                                nasapolydata, pwpolydata)


class Material {
public:
    Material() = default;


    // 从SQLite数据库行创建Material对象
    static Material fromSqlite(const std::string &dbRow);

    // 解析SCM格式的热力学数据
    void parseScmThermoData(const std::string &thermoBlock);

    // 获取属性
    const std::vector<MaterialProperty> &getProperty(const std::string &key) const {
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
    std::string chemical_formula;
    std::vector<std::string> speciesName;
    std::unordered_map<std::string, std::vector<MaterialProperty>> properties;

private:
    void parseScmTransportData(const std::string &transportBlock);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Material, name, type, description, chemical_formula, speciesName,
                                                properties)

