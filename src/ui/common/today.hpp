#pragma once

#include <ctime>
#include <string>

namespace nuvio::ui
{

// The local date as "YYYY-MM-DD", for episode release checks.
inline std::string today()
{
    std::time_t now = std::time(nullptr);
    char buffer[16];
    std::strftime(buffer, sizeof buffer, "%Y-%m-%d", std::localtime(&now));
    return buffer;
}

} // namespace nuvio::ui
