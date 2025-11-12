#pragma once
#include <memory> // For std::unique_ptr

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    // virtual void undo() = 0; // Optional: if undo functionality is needed
};
