#pragma once

#include "scripting/XMLDOMWrapper.h"
#include <memory>
#include <quickjs.h>
#include <string>

namespace RSM {

/**
 * W3C SCXML B.2: QuickJS bindings for XML DOM API
 * Creates JavaScript-accessible DOM objects with getElementsByTagName() and getAttribute()
 */
class DOMBinding {
public:
    /**
     * Create a JavaScript DOM object from XML content
     * Returns a JS object with getElementsByTagName() method
     */
    static JSValue createDOMObject(JSContext *ctx, const std::string &xmlContent);

private:
    // Wrapper to store XMLDocument in JS opaque data
    struct DOMObjectData {
        std::shared_ptr<XMLDocument> document;
        std::shared_ptr<XMLElement> element;
    };

    // Finalizer for DOM object
    static void domObjectFinalizer(JSRuntime *rt, JSValue val);

    // JavaScript method implementations
    static JSValue js_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue js_getAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

    // Helper to create element wrapper object
    static JSValue createElementObject(JSContext *ctx, std::shared_ptr<XMLElement> element);
};

}  // namespace RSM
