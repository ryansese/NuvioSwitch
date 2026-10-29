#include "core/log.hpp"

#include <cstdio>
#include <mutex>

namespace nuvio::core::log
{

namespace
{
std::mutex sinkMutex;
Sink currentSink;

void write(Level level, const std::string& message)
{
    Sink sink;
    {
        std::lock_guard<std::mutex> lock(sinkMutex);
        sink = currentSink;
    }
    if (sink)
        sink(level, message);
    else if (level >= Level::Warning)
        std::fprintf(stderr, "%s\n", message.c_str());
}
} // namespace

void setSink(Sink sink)
{
    std::lock_guard<std::mutex> lock(sinkMutex);
    currentSink = std::move(sink);
}

void debug(const std::string& message) { write(Level::Debug, message); }
void info(const std::string& message) { write(Level::Info, message); }
void warning(const std::string& message) { write(Level::Warning, message); }
void error(const std::string& message) { write(Level::Error, message); }

} // namespace nuvio::core::log
