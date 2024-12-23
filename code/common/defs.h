#pragma once

enum class TargetOs { Windows, Mac, Linux };
constexpr TargetOs target_os =
#if _WIN32
    TargetOs::Windows;
#elif __APPLE__
    TargetOs::Mac;
#else
    TargetOs::Linux;
#endif
