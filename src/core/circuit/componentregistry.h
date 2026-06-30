#pragma once
#include "src/core/circuit/componentitem.h"
#include <vector>
#include <string>
#include <map>
#include <functional>

struct ComponentDefinition {
    std::string type_name;
    std::vector<std::string> detect_single;
    std::map<std::string, std::vector<std::string>> detect_multi;
    std::vector<std::string> detect_pattern;
    bool is_output;
    std::function<ComponentItem*(int pin, QGraphicsItem*)> create_item;
};

class ComponentRegistry {
public:
    static ComponentRegistry& instance();
    void register_component(const ComponentDefinition& comp);
    const ComponentDefinition* find_by_type(const std::string& type_name) const;
private:
    std::vector<ComponentDefinition> definitions_;
    
};