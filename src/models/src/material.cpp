#include "material.h"
#include <regex>



void Material::parseScmThermoData(const std::string& thermoBlock) {
    // 解析热力学数据块
    std::regex polyRegex(R"((\d+\.?\d*)\s+(\d+\.?\d*)\s+(\d+\.?\d*)\s+(\d+\.?\d*)\s+(\d+\.?\d*)\s+(\d+\.?\d*)\s+(\d+\.?\d*))");
    std::smatch match;
    
    if (std::regex_search(thermoBlock, match, polyRegex)) {
        PolynomialData poly;
        poly.coefficients = {
            std::stod(match[1].str()),
            std::stod(match[2].str()),
            std::stod(match[3].str()),
            std::stod(match[4].str()),
            std::stod(match[5].str()),
            std::stod(match[6].str()),
            std::stod(match[7].str())
        };
        
        // 解析温度范围
        std::regex tempRegex(R"(\((\d+\.?\d*)\s+(\d+\.?\d*)\))");
        if (std::regex_search(thermoBlock, match, tempRegex)) {

        }
        
//        properties["specific-heat"] .push_back( {
//            "specific-heat", "J/(kg·K)", poly, "polynomial"
//        });
    }
}

const PolynomialData &MaterialProperty::getPolydata() const
{
    return polydata;
}

const NASAPolynomialData &MaterialProperty::getNasaPolydata() const {
    return nasapolydata;
}

const polyPiecewiseLinearData &MaterialProperty::getPolyPiecewiseLinearData() const {
    return ppldata;
}

const PiecewisePolynomialData &MaterialProperty::getPiecewisePolyData() const {
    return pwpolydata;
}
