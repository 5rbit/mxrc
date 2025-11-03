#ifndef MXRC_RESOURCE_MANAGER_H
#define MXRC_RESOURCE_MANAGER_H

#include <mutex>
#include <string>
#include <unordered_set>

namespace mxrc {
namespace task_mission {

class ResourceManager {
public:
    static ResourceManager& getInstance() {
        static ResourceManager instance;
        return instance;
    }

    bool acquire(const std::string& resource);
    void release(const std::string& resource);

private:
    ResourceManager() = default;
    std::mutex mtx;
    std::unordered_set<std::string> locked_resources;
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_RESOURCE_MANAGER_H
