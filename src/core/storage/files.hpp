#pragma once

#include <optional>
#include <string>

namespace nuvio::core::storage
{

// Creates path and its parents. Returns false if it doesn't exist afterwards.
bool ensureDir(const std::string& path);

std::optional<std::string> readFile(const std::string& path);

bool fileExists(const std::string& path);

// Writes data to <path>.tmp, flushes it and renames it over path, so readers see
// the old or the new contents and never a partial file. Where rename can't
// replace an existing file (FAT on the Switch SD card), the old file is removed
// and the rename retried; a crash in that window leaves only <path>.tmp, which
// Store recovers on load.
bool atomicWrite(const std::string& path, const std::string& data);

} // namespace nuvio::core::storage
