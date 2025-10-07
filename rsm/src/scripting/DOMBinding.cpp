#include "scripting/DOMBinding.h"
#include "common/Logger.h"

namespace RSM {

// JavaScript class ID for DOM objects
static JSClassID js_dom_element_class_id = 0;

void DOMBinding::domObjectFinalizer(JSRuntime * /*rt*/, JSValue val) {
    DOMObjectData *data = static_cast<DOMObjectData *>(JS_GetOpaque(val, js_dom_element_class_id));
    if (data) {
        delete data;
    }
}

JSValue DOMBinding::js_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "getElementsByTagName requires 1 argument");
    }

    // Get tag name
    const char *tagName = JS_ToCString(ctx, argv[0]);
    if (!tagName) {
        return JS_EXCEPTION;
    }
    std::string tagNameStr(tagName);
    JS_FreeCString(ctx, tagName);

    // Get DOM object data
    DOMObjectData *data = static_cast<DOMObjectData *>(JS_GetOpaque(this_val, js_dom_element_class_id));
    if (!data) {
        return JS_ThrowTypeError(ctx, "Invalid DOM object");
    }

    // Search for elements
    std::vector<std::shared_ptr<XMLElement>> elements;
    if (data->document) {
        elements = data->document->getElementsByTagName(tagNameStr);
    } else if (data->element) {
        elements = data->element->getElementsByTagName(tagNameStr);
    }

    // Create JavaScript array
    JSValue jsArray = JS_NewArray(ctx);
    for (size_t i = 0; i < elements.size(); ++i) {
        JSValue elementObj = createElementObject(ctx, elements[i]);
        JS_SetPropertyUint32(ctx, jsArray, static_cast<uint32_t>(i), elementObj);
    }

    return jsArray;
}

JSValue DOMBinding::js_getAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "getAttribute requires 1 argument");
    }

    // Get attribute name
    const char *attrName = JS_ToCString(ctx, argv[0]);
    if (!attrName) {
        return JS_EXCEPTION;
    }
    std::string attrNameStr(attrName);
    JS_FreeCString(ctx, attrName);

    // Get DOM element data
    DOMObjectData *data = static_cast<DOMObjectData *>(JS_GetOpaque(this_val, js_dom_element_class_id));
    if (!data || !data->element) {
        return JS_ThrowTypeError(ctx, "Invalid DOM element");
    }

    // Get attribute value
    std::string attrValue = data->element->getAttribute(attrNameStr);
    return JS_NewString(ctx, attrValue.c_str());
}

JSValue DOMBinding::createElementObject(JSContext *ctx, std::shared_ptr<XMLElement> element) {
    // Initialize class if needed
    if (js_dom_element_class_id == 0) {
        JS_NewClassID(JS_GetRuntime(ctx), &js_dom_element_class_id);
        JSClassDef classDef = {
            .class_name = "DOMElement",
            .finalizer = domObjectFinalizer,
            .gc_mark = nullptr,
            .call = nullptr,
            .exotic = nullptr,
        };
        JS_NewClass(JS_GetRuntime(ctx), js_dom_element_class_id, &classDef);
    }

    // Create object
    JSValue obj = JS_NewObjectClass(ctx, js_dom_element_class_id);
    if (JS_IsException(obj)) {
        return obj;
    }

    // Store element data
    DOMObjectData *data = new DOMObjectData();
    data->element = element;
    JS_SetOpaque(obj, data);

    // Add methods
    JS_SetPropertyStr(ctx, obj, "getElementsByTagName",
                      JS_NewCFunction(ctx, js_getElementsByTagName, "getElementsByTagName", 1));
    JS_SetPropertyStr(ctx, obj, "getAttribute", JS_NewCFunction(ctx, js_getAttribute, "getAttribute", 1));

    return obj;
}

JSValue DOMBinding::createDOMObject(JSContext *ctx, const std::string &xmlContent) {
    // Parse XML
    auto document = std::make_shared<XMLDocument>(xmlContent);
    if (!document->isValid()) {
        LOG_ERROR("DOMBinding: Failed to parse XML - {}", document->getErrorMessage());
        return JS_ThrowSyntaxError(ctx, "Failed to parse XML content");
    }

    // Initialize class if needed
    if (js_dom_element_class_id == 0) {
        JS_NewClassID(JS_GetRuntime(ctx), &js_dom_element_class_id);
        JSClassDef classDef = {
            .class_name = "DOMElement",
            .finalizer = domObjectFinalizer,
            .gc_mark = nullptr,
            .call = nullptr,
            .exotic = nullptr,
        };
        JS_NewClass(JS_GetRuntime(ctx), js_dom_element_class_id, &classDef);
    }

    // Create root object
    JSValue obj = JS_NewObjectClass(ctx, js_dom_element_class_id);
    if (JS_IsException(obj)) {
        return obj;
    }

    // Store document data
    DOMObjectData *data = new DOMObjectData();
    data->document = document;
    data->element = document->getDocumentElement();
    JS_SetOpaque(obj, data);

    // Add methods
    JS_SetPropertyStr(ctx, obj, "getElementsByTagName",
                      JS_NewCFunction(ctx, js_getElementsByTagName, "getElementsByTagName", 1));
    if (data->element) {
        JS_SetPropertyStr(ctx, obj, "getAttribute", JS_NewCFunction(ctx, js_getAttribute, "getAttribute", 1));
    }

    return obj;
}

}  // namespace RSM
