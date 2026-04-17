// Hand-written trampoline demonstrates the PYBIND11_OVERRIDE-equivalent
// pattern using mirror_bridge's TrampolineBase. The `mirror_bridge generate
// --trampolines` tool will emit code equivalent to Trampoline_Animal below
// for every class with virtual methods.
#include "mirror_bridge.hpp"
#include "src/shapes.hpp"

using namespace shapes;

class Trampoline_Animal : public Animal, public mirror_bridge::TrampolineBase<Animal> {
public:
    using Animal::Animal;

    std::string Sound() const override {
        if (mirror_bridge::has_python_override(this->py_self_, "Sound")) {
            return this->dispatch_python<std::string>("Sound");
        }
        throw std::runtime_error("Animal::Sound is pure virtual — Python override required");
    }

    std::string Name() const override {
        if (mirror_bridge::has_python_override(this->py_self_, "Name")) {
            return this->dispatch_python<std::string>("Name");
        }
        return Animal::Name();
    }
};

MIRROR_BRIDGE_MODULE(trampoline_test,
    mirror_bridge::bind_class<Animal, Trampoline_Animal>(m, "Animal");
    mirror_bridge::bind_class<Zoo>(m, "Zoo");
)
