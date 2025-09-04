// GuardUtils.h
#pragma once

#include <string>

namespace SCXML {
namespace Common {

class GuardUtils {
public:
    static bool isConditionExpression(const std::string &expression);
};

}  // namespace Common
}  // namespace SCXML
