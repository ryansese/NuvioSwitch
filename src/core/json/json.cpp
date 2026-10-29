#include "core/json/json.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>

namespace nuvio::core::json
{

namespace
{
const Json* field(const Json& j, const char* key)
{
    if (!j.is_object())
        return nullptr;
    auto it = j.find(key);
    if (it == j.end() || it->is_null())
        return nullptr;
    return &*it;
}

std::string trim(const std::string& s)
{
    size_t begin = 0;
    size_t end   = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(begin, end - begin);
}

// "name" of an object entry, as used for genres and cast in some add-ons.
std::optional<std::string> entryName(const Json& entry)
{
    if (entry.is_string())
        return entry.get<std::string>();
    if (entry.is_object())
        return optStr(entry, "name");
    return std::nullopt;
}
} // namespace

std::string str(const Json& j, const char* key, const std::string& fallback)
{
    return optStr(j, key).value_or(fallback);
}

std::optional<std::string> optStr(const Json& j, const char* key)
{
    const Json* value = field(j, key);
    if (!value || !value->is_string())
        return std::nullopt;
    return value->get<std::string>();
}

std::vector<std::string> strList(const Json& j, const char* key)
{
    std::vector<std::string> raw;
    if (const Json* value = field(j, key))
    {
        if (value->is_array())
        {
            for (const auto& entry : *value)
                if (auto name = entryName(entry))
                    raw.push_back(*name);
        }
        else if (auto name = entryName(*value))
        {
            raw.push_back(*name);
        }
    }

    std::vector<std::string> result;
    for (const auto& s : raw)
    {
        std::string trimmed = trim(s);
        if (!trimmed.empty())
            result.push_back(std::move(trimmed));
    }
    return result;
}

std::optional<int64_t> optInt(const Json& j, const char* key)
{
    const Json* value = field(j, key);
    if (!value)
        return std::nullopt;
    if (value->is_number_integer())
        return value->get<int64_t>();
    if (value->is_number_float())
        return static_cast<int64_t>(value->get<double>());
    if (value->is_string())
    {
        // whole string must be an integer, like Kotlin's toIntOrNull
        std::string s = trim(value->get<std::string>());
        if (s.empty())
            return std::nullopt;
        char* end = nullptr;
        errno     = 0;
        long long parsed = std::strtoll(s.c_str(), &end, 10);
        if (errno != 0 || *end != '\0')
            return std::nullopt;
        return static_cast<int64_t>(parsed);
    }
    return std::nullopt;
}

std::optional<bool> optBool(const Json& j, const char* key)
{
    const Json* value = field(j, key);
    if (!value)
        return std::nullopt;
    if (value->is_boolean())
        return value->get<bool>();
    if (value->is_number())
        return value->get<double>() != 0;
    if (value->is_string())
    {
        std::string s = trim(value->get<std::string>());
        for (auto& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (s == "true")
            return true;
        if (s == "false")
            return false;
    }
    return std::nullopt;
}

} // namespace nuvio::core::json
