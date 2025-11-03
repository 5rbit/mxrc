#ifndef TASK_CONTEXT_H
#define TASK_CONTEXT_H

#include <map>
#include <string>
#include <any> // For C++17 to hold various types

namespace mxrc {
namespace task_mission {

/**
 * @brief Represents the context for a task, holding its input parameters and output results.
 *
 * This class uses std::any to allow for flexible storage of different data types,
 * making it suitable for various task requirements. Users should cast the retrieved
 * std::any to the expected type.
 */
class TaskContext {
public:
    /**
     * @brief Default constructor.
     */
    TaskContext() = default;

    /**
     * @brief Sets a parameter in the context.
     *
     * @tparam T The type of the value.
     * @param key The string key associated with the parameter.
     * @param value The value to store.
     */
    template<typename T>
    void setParameter(const std::string& key, const T& value) {
        parameters_[key] = value;
    }

    /**
     * @brief Gets a parameter from the context.
     *
     * @tparam T The expected type of the value.
     * @param key The string key associated with the parameter.
     * @return The value cast to the specified type T.
     * @throws std::bad_any_cast if the stored type does not match T.
     * @throws std::out_of_range if the key is not found.
     */
    template<typename T>
    T getParameter(const std::string& key) const {
        if (parameters_.count(key) == 0) {
            throw std::out_of_range("Parameter not found: " + key);
        }
        return std::any_cast<T>(parameters_.at(key));
    }

    /**
     * @brief Checks if a parameter exists in the context.
     *
     * @param key The string key associated with the parameter.
     * @return True if the parameter exists, false otherwise.
     */
    bool hasParameter(const std::string& key) const {
        return parameters_.count(key) > 0;
    }

    /**
     * @brief Removes a parameter from the context.
     *
     * @param key The string key associated with the parameter.
     */
    void removeParameter(const std::string& key) {
        parameters_.erase(key);
    }

private:
    std::map<std::string, std::any> parameters_;
};

} // namespace task_mission
} // namespace mxrc

#endif // TASK_CONTEXT_H
