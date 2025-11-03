#ifndef MISSION_PARSER_H
#define MISSION_PARSER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp> // Assuming nlohmann/json for JSON parsing

namespace mxrc {
namespace task_mission {

// Forward declaration of MissionDefinition structure/class if it's defined elsewhere
// For now, we'll assume a simple structure for parsing purposes.
struct MissionDefinition {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    nlohmann::json behavior_tree; // Store as raw JSON for now
};

class MissionParser {
public:
    /**
     * @brief Parses a mission definition from a file.
     * @param filepath The path to the mission definition file (JSON or YAML).
     * @return A MissionDefinition object if parsing is successful, otherwise an empty/invalid object.
     */
    MissionDefinition parseMissionDefinition(const std::string& filepath);

    /**
     * @brief Validates the structure and content of a MissionDefinition.
     * @param missionDef The MissionDefinition object to validate.
     * @return True if the definition is valid, false otherwise.
     */
    bool validateMissionDefinition(const MissionDefinition& missionDef);

private:
    // Helper to parse JSON
    MissionDefinition parseJson(const std::string& content);

    // Helper to parse YAML (if supported)
    // MissionDefinition parseYaml(const std::string& content);
};

} // namespace task_mission
} // namespace mxrc

#endif // MISSION_PARSER_H
