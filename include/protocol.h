#pragma once
#include <string>
#include <jsoncpp/json/json.h>
#include "fields.hpp"

// serialize / unserialize 放在 JSON 命名空间里
// 避免和类成员函数 serialize（多态接口）重名冲突
namespace JSON {
    // Json::Value → std::string
    bool serialize(const Json::Value &val, std::string &body);
    // std::string → Json::Value
    bool unserialize(const std::string &body, Json::Value &val);
}