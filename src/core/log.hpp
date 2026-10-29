#pragma once

#include <functional>
#include <string>

namespace nuvio::core::log
{

enum class Level
{
    Debug,
    Info,
    Warning,
    Error,
};

using Sink = std::function<void(Level, const std::string&)>;

// Routes core logging somewhere; main.cpp points it at brls::Logger.
// Without a sink, warnings and errors go to stderr.
void setSink(Sink sink);

void debug(const std::string& message);
void info(const std::string& message);
void warning(const std::string& message);
void error(const std::string& message);

} // namespace nuvio::core::log
