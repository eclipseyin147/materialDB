#include "scm_parser.h"

#include "scheme.h"
#include "scheme-private.h"
#include <algorithm>
#include <regex>

namespace CFD_MaterialDB {
    std::vector<Material> ScmParser::parse(const std::string &filename) {

        scheme *sc = scheme_init_new();
        if (!sc) return {};

        std::vector<Material> materials;
        FILE *fin = nullptr;
        errno_t err = fopen_s(&fin, filename.c_str(), "r");
        if (err != 0 || !fin) {
            scheme_deinit(sc);
            return materials;
        }
        scheme_load_file(sc, fin);
        fclose(fin);
        pointer data = scheme_eval(sc, mk_symbol(sc, "data"));
        if (data == sc->NIL) {
            scheme_deinit(sc);
            return materials;
        }

        pointer parse_func = scheme_eval(sc, mk_symbol(sc, "parse-materials"));
        if (parse_func == sc->NIL) {
            scheme_deinit(sc);
            return materials;
        }

        pointer materials_list = scheme_apply0(sc, "parse-materials");
        if (materials_list == sc->NIL) {
            scheme_deinit(sc);
            return materials;
        }

        size_t length = sc->vptr->vector_length(materials_list);

        std::smatch match;
        std::regex nameTypeRegex(R"(\(name\s+\.\s+([^\s]+)\s+\(type\s+\.\s+([^\)]+)\))");

        for (size_t i = 0; i < length; ++i) {
            pointer material_data = sc->vptr->vector_elem(materials_list, i);
            Material material;

            // 解析材料名称和类型
            std::string fileContent = sc->vptr->string_value(data);
            if (std::regex_search(fileContent, match, nameTypeRegex)) {
                material.name = match[1].str();
                std::string typeStr = match[2].str();
                std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);

                if (typeStr == "FLUID") {
                    material.type.state = MaterialState::FLUID;
                    material.type.particle_flags = 0;
                } else if (typeStr == "SOLID") {
                    material.type.state = MaterialState::SOLID;
                    material.type.particle_flags = 0;
                } else if (typeStr == "MIXTURE") {
                    material.type.state = MaterialState::MIXTURE;
                    material.type.particle_flags = 0;
                } else if (typeStr == "COMBUSTION") {
                    material.type.state = MaterialState::SOLID;
                    material.type.particle_flags = 0;
                    material.type.addParticleType(ParticleType::COMBUSTING_PARTICLE);
                } else if (typeStr == "INERT-PARTICLE") {
                    material.type.state = MaterialState::SOLID;
                    material.type.particle_flags = 0;
                    material.type.addParticleType(ParticleType::INERT_PARTICLE);
                } else if (typeStr == "DROPLET-PARTICLE") {
                    material.type.state = MaterialState::SOLID;
                    material.type.particle_flags = 0;
                    material.type.addParticleType(ParticleType::DROPLET_PARTICLE);
                }

                // 解析化学式
                std::regex formulaRegex(R"(\(chemical-formula\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, formulaRegex)) {
                    material.properties["chemical-formula"] = {
                            "chemical-formula", "", match[1].str(), "string"
                    };
                }

                // 解析分子量
                std::regex weightRegex(R"(\(molecular-weight\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, weightRegex)) {
                    material.properties["molecular-weight"] = {
                            "molecular-weight", "g/mol", std::stod(match[1].str()), "number"
                    };
                }

                // 解析热力学数据
                std::regex thermoRegex(R"((specific-heat\s+\(polynomial\s+[^\)]+\)\s+\([^\)]+\)))");
                if (std::regex_search(filename, match, thermoRegex)) {
                    std::string thermoBlock = match[0].str();
                    material.parseScmThermoData(thermoBlock);
                }

                // 解析混合物组分
                std::regex speciesRegex(R"(\(species\s+\(names\s+([^\)]+)\))");
                if (std::regex_search(filename, match, speciesRegex)) {
                    material.properties["species"] = {
                            "species", "", match[1].str(), "string"
                    };
                }

                // 解析反应式
                std::regex reactionRegex(R"((reactions\s+\([^\)]+\)))");
                if (std::regex_search(filename, match, reactionRegex)) {
                    material.properties["reactions"] = {
                            "reactions", "", match[1].str(), "string"
                    };
                }

                // 解析临界属性
                std::regex criticalRegex(R"(\(critical-(pressure|temperature|volume)\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, criticalRegex)) {
                    material.properties["critical-" + match[1].str()] = {
                            "critical-" + match[1].str(), "", std::stod(match[2].str()), "number"
                    };
                }

                // 解析热导率
                std::regex thermalCondRegex(R"(\(thermal-conductivity\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, thermalCondRegex)) {
                    material.properties["thermal-conductivity"] = {
                            "thermal-conductivity", "W/(m·K)", std::stod(match[1].str()), "number"
                    };
                }

                // 解析粘度
                std::regex viscosityRegex(R"(\(viscosity\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, viscosityRegex)) {
                    material.properties["viscosity"] = {
                            "viscosity", "Pa·s", std::stod(match[1].str()), "number"
                    };
                }

                // 解析特征振动温度
                std::regex vibTempRegex(R"(\(characteristic-vibrational-temperature\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, vibTempRegex)) {
                    material.properties["characteristic-vibrational-temperature"] = {
                            "characteristic-vibrational-temperature", "K", std::stod(match[1].str()), "number"
                    };
                }

                // 解析生成焓
                std::regex formationEnthalpyRegex(R"(\(formation-enthalpy\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, formationEnthalpyRegex)) {
                    material.properties["formation-enthalpy"] = {
                            "formation-enthalpy", "J/mol", std::stod(match[1].str()), "number"
                    };
                }

                // 解析生成熵
                std::regex formationEntropyRegex(R"(\(formation-entropy\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, formationEntropyRegex)) {
                    material.properties["formation-entropy"] = {
                            "formation-entropy", "J/(mol·K)", std::stod(match[1].str()), "number"
                    };
                }

                // 解析潜热
                std::regex latentHeatRegex(R"(\(latent-heat\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, latentHeatRegex)) {
                    material.properties["latent-heat"] = {
                            "latent-heat", "J/kg", std::stod(match[1].str()), "number"
                    };
                }

                // 解析汽化温度
                std::regex vaporizationTempRegex(R"(\(vaporization-temperature\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, vaporizationTempRegex)) {
                    material.properties["vaporization-temperature"] = {
                            "vaporization-temperature", "K", std::stod(match[1].str()), "number"
                    };
                }

                // 解析沸点
                std::regex boilingPointRegex(R"(\(boiling-point\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, boilingPointRegex)) {
                    material.properties["boiling-point"] = {
                            "boiling-point", "K", std::stod(match[1].str()), "number"
                    };
                }

                // 解析二元扩散系数
                std::regex binaryDiffusivityRegex(R"(\(binary-diffusivity\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, binaryDiffusivityRegex)) {
                    material.properties["binary-diffusivity"] = {
                            "binary-diffusivity", "m²/s", std::stod(match[1].str()), "number"
                    };
                }

                // 解析挥发分
                std::regex volatileFractionRegex(R"(\(volatile-fraction\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, volatileFractionRegex)) {
                    material.properties["volatile-fraction"] = {
                            "volatile-fraction", "", std::stod(match[1].str()), "number"
                    };
                }

                // 解析热适应系数
                std::regex thermalAccomRegex(R"(\(thermal-accom-coefficient\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, thermalAccomRegex)) {
                    material.properties["thermal-accom-coefficient"] = {
                            "thermal-accom-coefficient", "", std::stod(match[1].str()), "number"
                    };
                }

                // 解析速度适应系数
                std::regex velocityAccomRegex(R"(\(velocity-accom-coefficient\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, velocityAccomRegex)) {
                    material.properties["velocity-accom-coefficient"] = {
                            "velocity-accom-coefficient", "", std::stod(match[1].str()), "number"
                    };
                }

                // 解析偏心因子
                std::regex acentricFactorRegex(R"(\(acentric-factor\s+\(constant\s+\.\s+([^\)]+)\))");
                if (std::regex_search(filename, match, acentricFactorRegex)) {
                    material.properties["acentric-factor"] = {
                            "acentric-factor", "", std::stod(match[1].str()), "number"
                    };
                }

                materials.push_back(material);
            }
            return materials;
        }


    }

}