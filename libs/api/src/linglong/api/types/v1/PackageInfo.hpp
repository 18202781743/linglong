// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     PackageInfo.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

#include "linglong/api/types/v1/ApplicationConfigurationPermissions.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* this is the each item output of ll-cli list --json
*/

using nlohmann::json;

/**
* this is the each item output of ll-cli list --json
*/
struct PackageInfo {
/**
* appid of package info (deprecated in V2)
*/
std::string appid;
/**
* arch of package info
*/
std::vector<std::string> arch;
/**
* base of package info
*/
std::string base;
/**
* channel of package info
*/
std::optional<std::string> channel;
/**
* command of package info
*/
std::optional<std::vector<std::string>> command;
/**
* description of package info
*/
std::optional<std::string> description;
/**
* kind of package info
*/
std::string kind;
/**
* module of package info
*/
std::string packageInfoModule;
/**
* name of package info
*/
std::string name;
std::optional<ApplicationConfigurationPermissions> permissions;
/**
* runtime of package info
*/
std::optional<std::string> runtime;
/**
* Uncompressed package size in bytes
*/
int64_t size;
/**
* version of package info
*/
std::string version;
};
}
}
}
}

// clang-format on
