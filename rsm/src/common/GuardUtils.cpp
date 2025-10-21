#include "GuardUtils.h"

namespace RSM {
namespace GuardUtils {

bool isConditionExpression(const std::string &expression) {
    // Check for operators commonly found in conditional expressions
    return expression.find('>') != std::string::npos || expression.find('<') != std::string::npos ||
           expression.find('=') != std::string::npos || expression.find('!') != std::string::npos ||
           expression.find('+') != std::string::npos || expression.find('-') != std::string::npos ||
           expression.find('*') != std::string::npos || expression.find('/') != std::string::npos;
}

}  // namespace GuardUtils
}  // namespace RSM
