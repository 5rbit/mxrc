#include "MissionParser.h"
#include <fstream>
#include <iostream>

// Assuming nlohmann/json is available. If not, it needs to be added to CMakeLists.txt
// #include <nlohmann/json.hpp>

namespace mxrc {
namespace task {

MissionDefinition MissionParser::parseMissionDefinition(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open mission definition file: " << filepath << std::endl;
        return {}; // Return empty MissionDefinition
    }

    nlohmann::json json_data;
    try {
        file >> json_data;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Error parsing mission definition JSON from " << filepath << ": " << e.what() << std::endl;
        return {};
    }

    MissionDefinition missionDef;
    missionDef.id = json_data.value("id", "");
    missionDef.name = json_data.value("name", "");
    missionDef.version = json_data.value("version", "");
    missionDef.description = json_data.value("description", "");
    missionDef.behavior_tree = json_data.value("behavior_tree", nlohmann::json::object());

    return missionDef;
}

bool MissionParser::validateMissionDefinition(const MissionDefinition& missionDef) {
    if (missionDef.id.empty()) {
        std::cerr << "Validation Error: Mission ID is empty." << std::endl;
        return false;
    }
    if (missionDef.name.empty()) {
        std::cerr << "Validation Error: Mission name is empty." << std::endl;
        return false;
    }
    if (missionDef.version.empty()) {
        std::cerr << "Validation Error: Mission version is empty." << std::endl;
        return false;
    }
    if (missionDef.behavior_tree.empty() || !missionDef.behavior_tree.contains("root") || !missionDef.behavior_tree.contains("nodes")) {
        std::cerr << "Validation Error: Behavior tree structure is invalid." << std::endl;
        return false;
    }

    // Further validation of behavior_tree structure can be added here
    // e.g., checking node types, children, parameters, etc.

    return true;
}

} // namespace task
} // namespace mxrc
