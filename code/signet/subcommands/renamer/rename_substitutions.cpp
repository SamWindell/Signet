#include "rename_substitutions.h"

namespace RenameSubstitution {

std::string GetFullInfo() {
    std::string result = "\n\n";
    for (const auto v : g_vars) {
        result += v.name;
        result += "\n";
        result += v.desc;
        result += "\n\n";
    }
    result.resize(result.size() - 2);
    return result;
}

std::string GetVariableNames() {
    std::string result {};
    for (const auto v : g_vars) {
        result += v.name;
        result += "\n";
    }
    result.resize(result.size() - 1);
    return result;
}

} // namespace RenameSubstitution
