#pragma once
#include "src/core/circuit/componentitem.h"
#include <QColor>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <functional>

class CircuitDetector;

struct PinRole {
    std::string name;
    std::vector<std::string> keywords;
};

enum class MultiPinStrategy {
    None,
    Suffix,
    Prefix,
    Array,
    Singleton,
};

struct ComponentDefinition {
    std::string type_name;
    std::vector<std::string> detect_single;
    std::vector<PinRole> detect_multi;
    std::vector<std::string> detect_pattern;
    bool is_output;
    std::function<ComponentItem*(int pin, QGraphicsItem*)> create_item;
    MultiPinStrategy multi_pin_strategy = MultiPinStrategy::None;
    std::string representative_role;
    QColor wire_color = QColor("#888888");

    // Escape hatch for detection syntax the generic engine can't express.
    std::function<void(CircuitDetector& ctx, const std::string& source,
                        const std::map<std::string, std::string>& defines,
                        const std::map<std::string, std::vector<int>>& arrays,
                        std::set<int>& claimed)> detect_custom;
};

class ComponentRegistry {
public:
    static ComponentRegistry& instance();
    void register_component(const ComponentDefinition& comp);
    const ComponentDefinition* find_by_type(const std::string& type_name) const;
    const ComponentDefinition* find_by_single_keyword(const std::string& upper_name) const;
    const std::vector<ComponentDefinition>& all() const { return definitions_; }
private:
    std::vector<ComponentDefinition> definitions_;
    
};