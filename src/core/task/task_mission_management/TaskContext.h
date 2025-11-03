#ifndef MXRC_TASK_CONTEXT_H
#define MXRC_TASK_CONTEXT_H

#include <any>
#include <map>
#include <string>
#include <stdexcept> // For std::runtime_error

namespace mxrc {
namespace task_mission {

class TaskContext {
public:
    // Set a value in the context
    template<typename T>
    void set(const std::string& key, const T& value) {
        data_[key] = value;
    }

    // Get a value from the context
    template<typename T>
    T get(const std::string& key) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast& e) {
                throw std::runtime_error("TaskContext: Type mismatch for key '" + key + "': " + e.what());
            }
        }
        throw std::runtime_error("TaskContext: Key '" + key + "' not found.");
    }

    // Check if a key exists in the context
    bool contains(const std::string& key) const {
        return data_.count(key) > 0;
    }

    // Remove a key from the context
    void remove(const std::string& key) {
        data_.erase(key);
    }

private:
    std::map<std::string, std::any> data_;
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_TASK_CONTEXT_H