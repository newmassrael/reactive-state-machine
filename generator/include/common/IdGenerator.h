#ifndef IDGENERATOR_H
#define IDGENERATOR_H

#include <string>

namespace SCXML {
namespace Common {

class IdGenerator {
public:
    static std::string generateSessionId(const std::string &prefix = "scxml");
    static std::string generateEventId(const std::string &prefix = "event");
    static std::string generateUniqueId(const std::string &prefix = "id");

private:
    IdGenerator() = default;
};

}  // namespace Common
}  // namespace SCXML

// Compatibility support
using IdGenerator = SCXML::Common::IdGenerator;

#endif  // IDGENERATOR_H