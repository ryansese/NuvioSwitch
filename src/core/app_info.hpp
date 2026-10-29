#pragma once

#include <string>

namespace nuvio
{

// "<name> <version>", e.g. "NuvioSwitch 0.1.0".
std::string formatVersionText(const std::string& name, const std::string& version);

// The version text for this build, from the generated version.hpp.
std::string appVersionText();

} // namespace nuvio
