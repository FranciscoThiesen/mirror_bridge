#pragma once

// ============================================================================
// Mirror Bridge - Binding Customization Options
// ============================================================================
//
// Provides a builder-style API for customizing class bindings:
// - Exclude specific members from binding
// - Rename members for the target language
// - Mark properties as read-only
//
// Usage:
//   mirror_bridge::bind_class<MyClass>(m, "MyClass")
//       .exclude("internal_data")
//       .rename("cppName", "python_name")
//       .readonly("computed_value");
//
// ============================================================================

#include <string>
#include <unordered_set>
#include <unordered_map>

namespace mirror_bridge {

// ============================================================================
// Binding Options Storage
// ============================================================================

struct BindingOptions {
    // Members to exclude from binding (by C++ name)
    std::unordered_set<std::string> excluded_members;

    // Member renames: C++ name -> Python name
    std::unordered_map<std::string, std::string> renamed_members;

    // Members that should be read-only (no setter)
    std::unordered_set<std::string> readonly_members;

    // Check if a member should be excluded
    bool is_excluded(const std::string& name) const {
        return excluded_members.count(name) > 0;
    }

    // Get the Python name for a member (handles renames)
    std::string get_python_name(const std::string& cpp_name) const {
        auto it = renamed_members.find(cpp_name);
        return (it != renamed_members.end()) ? it->second : cpp_name;
    }

    // Check if a member should be read-only
    bool is_readonly(const std::string& name) const {
        return readonly_members.count(name) > 0;
    }
};

// ============================================================================
// Type-specific Options Registry
// ============================================================================

// Global registry for binding options (indexed by type name)
class OptionsRegistry {
public:
    static OptionsRegistry& instance() {
        static OptionsRegistry registry;
        return registry;
    }

    BindingOptions& get_options(const std::string& type_name) {
        return options_[type_name];
    }

    const BindingOptions* find_options(const std::string& type_name) const {
        auto it = options_.find(type_name);
        return (it != options_.end()) ? &it->second : nullptr;
    }

    void clear() {
        options_.clear();
    }

private:
    OptionsRegistry() = default;
    std::unordered_map<std::string, BindingOptions> options_;
};

// ============================================================================
// Builder Class for Fluent API
// ============================================================================

template<typename T>
class ClassBinder {
public:
    ClassBinder(PyObject* module, const char* name, PyTypeObject* type)
        : module_(module), name_(name), type_(type) {}

    // Exclude a member from binding
    ClassBinder& exclude(const char* member_name) {
        OptionsRegistry::instance().get_options(name_).excluded_members.insert(member_name);
        return *this;
    }

    // Exclude multiple members
    ClassBinder& exclude(std::initializer_list<const char*> members) {
        auto& opts = OptionsRegistry::instance().get_options(name_);
        for (const char* m : members) {
            opts.excluded_members.insert(m);
        }
        return *this;
    }

    // Rename a member for Python
    ClassBinder& rename(const char* cpp_name, const char* python_name) {
        OptionsRegistry::instance().get_options(name_).renamed_members[cpp_name] = python_name;
        return *this;
    }

    // Mark a member as read-only
    ClassBinder& readonly(const char* member_name) {
        OptionsRegistry::instance().get_options(name_).readonly_members.insert(member_name);
        return *this;
    }

    // Mark multiple members as read-only
    ClassBinder& readonly(std::initializer_list<const char*> members) {
        auto& opts = OptionsRegistry::instance().get_options(name_);
        for (const char* m : members) {
            opts.readonly_members.insert(m);
        }
        return *this;
    }

    // Get the underlying PyTypeObject
    PyTypeObject* type() const { return type_; }

    // Implicit conversion to PyTypeObject* for compatibility
    operator PyTypeObject*() const { return type_; }

private:
    PyObject* module_;
    std::string name_;
    PyTypeObject* type_;
};

// ============================================================================
// Static Options (compile-time configuration via traits)
// ============================================================================

// Users can specialize this template to provide compile-time options
template<typename T>
struct type_options {
    // Override in specializations:
    // static constexpr bool exclude_member(std::string_view name) { return false; }
    // static constexpr std::string_view rename_member(std::string_view name) { return name; }
    // static constexpr bool is_readonly(std::string_view name) { return false; }
};

// Helper to check if type_options is specialized for T
template<typename T, typename = void>
struct has_type_options : std::false_type {};

template<typename T>
struct has_type_options<T, std::void_t<decltype(type_options<T>::exclude_member)>> : std::true_type {};

} // namespace mirror_bridge
