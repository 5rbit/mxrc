#include "ResourceManager.h"

namespace mxrc {
namespace task_mission {

bool ResourceManager::acquire(const std::string& resource) {
    std::lock_guard<std::mutex> lock(mtx);
    return locked_resources.insert(resource).second;
}

void ResourceManager::release(const std::string& resource) {
    std::lock_guard<std::mutex> lock(mtx);
    locked_resources.erase(resource);
}

} // namespace task_mission
} // namespace mxrc
