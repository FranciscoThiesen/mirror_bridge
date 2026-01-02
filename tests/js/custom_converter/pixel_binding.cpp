// JavaScript binding for Pixel with custom Color3 converter
// Demonstrates the custom type converter extension point for JavaScript/Node.js

#include "color3.hpp"

// Include Node.js N-API first
#include <node_api.h>

// Forward declare the custom converter BEFORE including mirror_bridge_javascript
namespace mirror_bridge::javascript {

// Define to_javascript/from_javascript for Color3 type
// Converts Color3 <-> JavaScript Array [r, g, b]

inline napi_value to_javascript(napi_env env, const Color3& color) {
    napi_value result;
    napi_create_array_with_length(env, 3, &result);

    napi_value r, g, b;
    napi_create_uint32(env, color.r, &r);
    napi_create_uint32(env, color.g, &g);
    napi_create_uint32(env, color.b, &b);

    napi_set_element(env, result, 0, r);
    napi_set_element(env, result, 1, g);
    napi_set_element(env, result, 2, b);

    return result;
}

inline bool from_javascript(napi_env env, napi_value val, Color3& color) {
    bool is_array;
    napi_is_array(env, val, &is_array);

    if (is_array) {
        // Handle Array [r, g, b]
        uint32_t length;
        napi_get_array_length(env, val, &length);
        if (length != 3) return false;

        napi_value r_val, g_val, b_val;
        napi_get_element(env, val, 0, &r_val);
        napi_get_element(env, val, 1, &g_val);
        napi_get_element(env, val, 2, &b_val);

        uint32_t r, g, b;
        napi_get_value_uint32(env, r_val, &r);
        napi_get_value_uint32(env, g_val, &g);
        napi_get_value_uint32(env, b_val, &b);

        color.r = static_cast<uint8_t>(r);
        color.g = static_cast<uint8_t>(g);
        color.b = static_cast<uint8_t>(b);
        return true;
    }

    // Handle Object {r, g, b}
    napi_valuetype type;
    napi_typeof(env, val, &type);
    if (type == napi_object) {
        napi_value r_val, g_val, b_val;
        bool has_r, has_g, has_b;

        napi_has_named_property(env, val, "r", &has_r);
        napi_has_named_property(env, val, "g", &has_g);
        napi_has_named_property(env, val, "b", &has_b);

        if (has_r && has_g && has_b) {
            napi_get_named_property(env, val, "r", &r_val);
            napi_get_named_property(env, val, "g", &g_val);
            napi_get_named_property(env, val, "b", &b_val);

            uint32_t r, g, b;
            napi_get_value_uint32(env, r_val, &r);
            napi_get_value_uint32(env, g_val, &g);
            napi_get_value_uint32(env, b_val, &b);

            color.r = static_cast<uint8_t>(r);
            color.g = static_cast<uint8_t>(g);
            color.b = static_cast<uint8_t>(b);
            return true;
        }
    }

    return false;
}

} // namespace mirror_bridge::javascript

// Now include mirror_bridge_javascript - it will find our custom converters
#include "javascript/mirror_bridge_javascript.hpp"

MIRROR_BRIDGE_JS_MODULE(pixel,
    mirror_bridge::javascript::bind_class<Pixel>(env, m, "Pixel");
)
