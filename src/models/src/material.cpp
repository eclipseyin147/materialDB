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
            poly.min_temp = std::stod(match[1].str());
            poly.max_temp = std::stod(match[2].str());
        }
        
        properties["specific-heat"] = {
            "specific-heat", "J/(kg·K)", poly, "polynomial"
        };
    }
}

