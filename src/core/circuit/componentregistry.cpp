#include "componentregistry.h"

ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry registry;
    return registry;
}

void ComponentRegistry::register_component(const ComponentDefinition& comp) {
    definitions_.push_back(comp);
}

const ComponentDefinition* ComponentRegistry::find_by_type(const std::string& type_name) const {
    for (const auto& def : definitions_) {
        if (def.type_name == type_name)
            return &def;
    }
    return nullptr;
}
