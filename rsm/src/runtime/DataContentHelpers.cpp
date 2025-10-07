#include <algorithm>
#include <string>

namespace RSM {

// W3C SCXML B.2: Helper function to normalize whitespace in text content
std::string normalizeWhitespace(const std::string &text) {
    std::string trimmed = text;

    // Trim leading/trailing whitespace
    size_t start = trimmed.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";  // All whitespace
    }
    trimmed = trimmed.substr(start);
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    if (end != std::string::npos) {
        trimmed = trimmed.substr(0, end + 1);
    }

    // Replace sequences of whitespace with single space
    bool inWhitespace = false;
    std::string result;
    result.reserve(trimmed.length());

    for (char c : trimmed) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!inWhitespace) {
                result += ' ';
                inWhitespace = true;
            }
        } else {
            result += c;
            inWhitespace = false;
        }
    }

    // Trim trailing space if added
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

// W3C SCXML B.2: Helper function to detect if content is XML
bool isXMLContent(const std::string &content) {
    if (content.empty()) {
        return false;
    }

    std::string trimmed = content;
    size_t start = trimmed.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return false;
    }
    return trimmed[start] == '<';
}

}  // namespace RSM
