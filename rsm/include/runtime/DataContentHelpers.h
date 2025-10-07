#pragma once

#include <string>

namespace RSM {

// W3C SCXML B.2: Helper functions for data content processing

// Normalize whitespace in text content (test 558)
std::string normalizeWhitespace(const std::string &text);

// Detect if content is XML (test 557)
bool isXMLContent(const std::string &content);

}  // namespace RSM
