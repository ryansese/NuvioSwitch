#include "core/app_info.hpp"

#include "version.hpp"

namespace nuvio
{

std::string formatVersionText(const std::string& name, const std::string& version)
{
    if (version.empty())
        return name;
    return name + " " + version;
}

std::string appVersionText()
{
    return formatVersionText(NUVIO_APP_NAME, NUVIO_APP_VERSION);
}

} // namespace nuvio
