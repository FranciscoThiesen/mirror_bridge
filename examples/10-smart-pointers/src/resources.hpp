#pragma once
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

// Example: Smart Pointers
// Demonstrates shared_ptr and unique_ptr handling

namespace resources {

// Simple resource class
class Resource {
public:
    std::string name;
    int id = 0;
    bool active = true;

    Resource() = default;
    Resource(const std::string& n, int i) : name(n), id(i) {}

    std::string describe() const {
        return name + " (#" + std::to_string(id) + ")" +
               (active ? " [active]" : " [inactive]");
    }

    void deactivate() {
        active = false;
    }
};

// Class holding shared resources
class ResourcePool {
public:
    std::vector<std::shared_ptr<Resource>> resources;

    void add(std::shared_ptr<Resource> res) {
        resources.push_back(res);
    }

    std::shared_ptr<Resource> get(int id) const {
        for (const auto& res : resources) {
            if (res->id == id) {
                return res;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Resource> get_by_name(const std::string& name) const {
        for (const auto& res : resources) {
            if (res->name == name) {
                return res;
            }
        }
        return nullptr;
    }

    int count() const {
        return static_cast<int>(resources.size());
    }

    int active_count() const {
        int n = 0;
        for (const auto& res : resources) {
            if (res->active) n++;
        }
        return n;
    }

    void clear() {
        resources.clear();
    }
};

// Factory that creates shared resources
class ResourceFactory {
private:
    int next_id = 1;

public:
    std::shared_ptr<Resource> create(const std::string& name) {
        return std::make_shared<Resource>(name, next_id++);
    }

    std::shared_ptr<Resource> create_inactive(const std::string& name) {
        auto res = create(name);
        res->active = false;
        return res;
    }

    int total_created() const {
        return next_id - 1;
    }

    void reset() {
        next_id = 1;
    }
};

// Document with ownership semantics
class Document {
public:
    std::string title;
    std::string content;

    Document() = default;
    Document(const std::string& t, const std::string& c)
        : title(t), content(c) {}

    int word_count() const {
        if (content.empty()) return 0;
        int count = 0;
        bool in_word = false;
        for (char ch : content) {
            if (std::isspace(ch)) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                count++;
            }
        }
        return count;
    }
};

// Editor that holds a unique document (exclusive ownership)
class Editor {
private:
    std::unique_ptr<Document> doc;

public:
    bool has_document() const {
        return doc != nullptr;
    }

    void new_document(const std::string& title) {
        doc = std::make_unique<Document>(title, "");
    }

    void load_document(std::unique_ptr<Document> d) {
        doc = std::move(d);
    }

    std::unique_ptr<Document> release_document() {
        return std::move(doc);
    }

    std::string get_title() const {
        if (!doc) return "";
        return doc->title;
    }

    std::string get_content() const {
        if (!doc) return "";
        return doc->content;
    }

    void set_content(const std::string& content) {
        if (doc) {
            doc->content = content;
        }
    }

    void append(const std::string& text) {
        if (doc) {
            doc->content += text;
        }
    }

    int word_count() const {
        if (!doc) return 0;
        return doc->word_count();
    }

    void close() {
        doc.reset();
    }
};

// Node for tree structures (shared children)
class TreeNode {
public:
    std::string value;
    std::vector<std::shared_ptr<TreeNode>> children;
    std::weak_ptr<TreeNode> parent;  // Weak to avoid cycles

    explicit TreeNode(const std::string& v) : value(v) {}

    void add_child(std::shared_ptr<TreeNode> child) {
        children.push_back(child);
    }

    int child_count() const {
        return static_cast<int>(children.size());
    }

    std::shared_ptr<TreeNode> get_child(int index) const {
        if (index < 0 || index >= static_cast<int>(children.size())) {
            return nullptr;
        }
        return children[index];
    }

    bool has_parent() const {
        return !parent.expired();
    }

    std::shared_ptr<TreeNode> get_parent() const {
        return parent.lock();
    }

    int depth() const {
        int d = 0;
        auto p = parent.lock();
        while (p) {
            d++;
            p = p->parent.lock();
        }
        return d;
    }
};

// Tree builder helper
class TreeBuilder {
public:
    static std::shared_ptr<TreeNode> create_node(const std::string& value) {
        return std::make_shared<TreeNode>(value);
    }

    static void add_child(std::shared_ptr<TreeNode> parent,
                          std::shared_ptr<TreeNode> child) {
        if (parent && child) {
            child->parent = parent;
            parent->add_child(child);
        }
    }

    static std::shared_ptr<TreeNode> build_sample_tree() {
        auto root = create_node("root");
        auto a = create_node("A");
        auto b = create_node("B");
        auto a1 = create_node("A1");
        auto a2 = create_node("A2");

        add_child(root, a);
        add_child(root, b);
        add_child(a, a1);
        add_child(a, a2);

        return root;
    }
};

// Handle class (opaque unique ownership)
class Handle {
private:
    int internal_id;
    bool valid = true;

public:
    explicit Handle(int id) : internal_id(id) {}

    int id() const { return internal_id; }
    bool is_valid() const { return valid; }

    void invalidate() { valid = false; }
};

// Manager that creates and tracks handles
class HandleManager {
private:
    int next_handle_id = 1;
    std::vector<std::unique_ptr<Handle>> handles;

public:
    Handle* create_handle() {
        handles.push_back(std::make_unique<Handle>(next_handle_id++));
        return handles.back().get();
    }

    int handle_count() const {
        return static_cast<int>(handles.size());
    }

    int valid_count() const {
        int n = 0;
        for (const auto& h : handles) {
            if (h->is_valid()) n++;
        }
        return n;
    }

    void invalidate_all() {
        for (auto& h : handles) {
            h->invalidate();
        }
    }

    void clear() {
        handles.clear();
    }
};

} // namespace resources
