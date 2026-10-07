/* A second native addon, standing in for any other npm native module loaded
 * into the same Node process.
 *
 * It exists because napi_unwrap is a weaker statement than it looks: it says
 * an object was napi_wrap'd, not by whom, and the private key it reads
 * belongs to the Node environment rather than to a module. So this addon's
 * wrapped objects come back out of napi_unwrap inside mirror_bridge just as
 * readily as mirror_bridge's own, with this addon's payload layout. The
 * payloads below put the values a real addon plausibly stores first - a null
 * handle, a small integer, a function pointer where a C++ object's vtable
 * would be - where mirror_bridge used to read a const char* and follow it.
 */
#include <node_api.h>
#include <cstddef>
#include <cstdlib>

struct Payload {
    void* first_word;
    double rest;
};

static void finalize(napi_env env, void* data, void* hint) {
    (void)env; (void)hint;
    std::free(data);
}

static napi_value wrapped_with(napi_env env, void* first_word) {
    napi_value obj;
    napi_create_object(env, &obj);
    auto* p = static_cast<Payload*>(std::calloc(1, sizeof(Payload)));
    if (!p) return obj;
    p->first_word = first_word;
    napi_wrap(env, obj, p, finalize, NULL, NULL);
    return obj;
}

static napi_value null_first(napi_env env, napi_callback_info info) {
    (void)info;
    return wrapped_with(env, NULL);
}

static napi_value small_first(napi_env env, napi_callback_info info) {
    (void)info;
    return wrapped_with(env, reinterpret_cast<void*>(static_cast<std::size_t>(7)));
}

static napi_value pointer_first(napi_env env, napi_callback_info info) {
    (void)info;
    return wrapped_with(env, reinterpret_cast<void*>(&finalize));
}

static napi_value Init(napi_env env, napi_value exports) {
    struct Entry { const char* name; napi_callback fn; };
    static const Entry entries[] = {
        {"nullFirst", null_first},
        {"smallFirst", small_first},
        {"pointerFirst", pointer_first},
    };
    for (unsigned i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        napi_value fn;
        napi_create_function(env, entries[i].name, NAPI_AUTO_LENGTH,
                             entries[i].fn, NULL, &fn);
        napi_set_named_property(env, exports, entries[i].name, fn);
    }
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
