// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     MinifiedInfo.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

#include "linglong/api/types/v1/Info.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* this is used to associate application with specific dependencies
*/

using nlohmann::json;

/**
* this is used to associate application with specific dependencies
*/
struct MinifiedInfo {
std::vector<Info> infos;
};
}
}
}
}

// clang-format on
