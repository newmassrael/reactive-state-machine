#include "common/GuardUtils.h"

namespace RSM {
namespace GuardUtils {

bool isConditionExpression(const std::string &expression) {
    // 일반적인 조건식에 포함된 연산자들 확인
    return expression.find('>') != std::string::npos ||
           expression.find('<') != std::string::npos ||
           expression.find('=') != std::string::npos ||
           expression.find('!') != std::string::npos ||
           expression.find('+') != std::string::npos ||
           expression.find('-') != std::string::npos ||
           expression.find('*') != std::string::npos ||
           expression.find('/') != std::string::npos;
}

}  // namespace GuardUtils
}  // namespace RSM
