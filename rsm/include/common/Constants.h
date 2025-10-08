#pragma once

/**
 * @file Constants.h
 * @brief Global constants for W3C SCXML specification compliance
 *
 * This file defines standard URIs and identifiers used throughout the SCXML implementation
 * to ensure consistency and compliance with W3C SCXML 1.0 specification.
 */

namespace RSM::Constants {

// ============================================================================
// W3C SCXML Event Processor URIs (W3C SCXML 6.2)
// ============================================================================

/**
 * @brief Standard SCXML Event Processor URI
 *
 * Default event processor for SCXML internal events.
 * Used when no type is specified in <send> elements.
 *
 * @see W3C SCXML 1.0 Section 6.2
 * @see W3C Test 200: Processors MUST support this type
 */
constexpr const char *SCXML_EVENT_PROCESSOR_URI = "http://www.w3.org/TR/scxml/#SCXMLEventProcessor";

/**
 * @brief Basic HTTP Event Processor URI
 *
 * Event processor for HTTP-based external communication.
 * Requires target attribute for destination URL.
 *
 * @see W3C SCXML 1.0 Appendix C.2
 * @see W3C Test 577: HTTP processor requires target attribute
 */
constexpr const char *BASIC_HTTP_EVENT_PROCESSOR_URI = "http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor";

// ============================================================================
// W3C SCXML Invoke Processor URIs (W3C SCXML 6.4)
// ============================================================================

/**
 * @brief Standard SCXML Invoke Processor URI
 *
 * Default invoke processor for SCXML sub-documents.
 * Platforms MUST support this type for invoke elements.
 *
 * @see W3C SCXML 1.0 Section 6.4
 * @see W3C Test 220: Processors MUST support this type
 */
constexpr const char *SCXML_INVOKE_PROCESSOR_URI = "http://www.w3.org/TR/scxml/";

}  // namespace RSM::Constants
