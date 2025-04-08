//
// Created by siqi on 4/8/25.
//
#include "material.h"

#ifndef CFD_MATERIALDB_SCM_PARSER_H
#define CFD_MATERIALDB_SCM_PARSER_H
namespace CFD_MaterialDB
{
    class ScmParser
    {
    public:
        ScmParser() =default;
        static std::vector<Material> parse(const std::string& filename);
    };

}
#endif //CFD_MATERIALDB_SCM_PARSER_H
