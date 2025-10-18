#pragma once

/**
 * @brief Single include file for all JIT tests
 *
 * Include this file to register all JIT tests.
 * Tests are automatically registered via inline static JitTestRegistrar instances.
 */

// Simple tests (basic pattern)
#include "Test144.h"
#include "Test147.h"
#include "Test148.h"
#include "Test149.h"
#include "Test150.h"
#include "Test151.h"
#include "Test152.h"
#include "Test153.h"
#include "Test155.h"
#include "Test156.h"
#include "Test158.h"
#include "Test159.h"
#include "Test172.h"
#include "Test173.h"
#include "Test174.h"
#include "Test176.h"
#include "Test179.h"
#include "Test183.h"
#include "Test193.h"
#include "Test194.h"
#include "Test200.h"
#include "Test276.h"
#include "Test277.h"
#include "Test278.h"
#include "Test279.h"
#include "Test280.h"
#include "Test286.h"
#include "Test287.h"
#include "Test301.h"
#include "Test311.h"
#include "Test312.h"
#include "Test313.h"
#include "Test314.h"

// Scheduled tests (event scheduler polling)
#include "Test175.h"
#include "Test185.h"
#include "Test186.h"
#include "Test208.h"

// Special tests (custom final states or logic)
#include "Test178.h"

// Note: Tests not listed here require Interpreter engine wrappers
// - Dynamic invoke tests: 187, 189-192, 205, 207, 210, 215-216, 220, etc.
// - Metadata-dependent tests: 198-199, 201 (require _event metadata or TypeRegistry)
// - Complex tests: 226, 239 (parent-child communication, hierarchical states)
// These use Interpreter wrappers due to runtime-only features (see ARCHITECTURE.md)
