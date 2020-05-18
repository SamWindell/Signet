#pragma once
#include <iostream>
#include <string_view>

template <typename Arg, typename... Args>
void FatalErrorWithNewLine(Arg&& arg, Args&&... args) {
    std::cout << "ERROR: ";
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
    exit(1);
}

std::string_view GetExtension(const std::string_view path) {
    const auto index = path.find_last_of('.');
    if (index == std::string::npos) {
        return path;
    }
    return path.substr(index);
}
