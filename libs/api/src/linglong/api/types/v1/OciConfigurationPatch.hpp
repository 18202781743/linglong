// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     OciConfigurationPatch.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* oci configuration patch
*/

using nlohmann::json;

/**
* oci configuration patch
*/
struct OciConfigurationPatch {
/**
* version of oci configuration patch
*/
std::string ociVersion;
/**
* oci configuration patch
*/
std::vector<nlohmann::json> patch;
};
}
}
}
}

// clang-format on
