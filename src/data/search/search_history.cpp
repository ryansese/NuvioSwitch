#include "data/search/search_history.hpp"

#include <algorithm>
#include <cctype>

namespace nuvio::data::search
{

namespace
{
std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}
} // namespace

std::string trimQuery(const std::string& query)
{
    size_t begin = 0;
    size_t end   = query.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(query[begin])))
        begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(query[end - 1])))
        end--;
    return query.substr(begin, end - begin);
}

bool isSearchableQuery(const std::string& query)
{
    // characters, not bytes: UTF-8 continuation bytes don't count
    size_t length = 0;
    for (unsigned char c : trimQuery(query))
        if ((c & 0xC0) != 0x80)
            length++;
    return length >= MIN_QUERY_LENGTH;
}

bool SearchHistory::add(const std::string& query)
{
    std::string trimmed = trimQuery(query);
    if (!isSearchableQuery(trimmed))
        return false;

    std::string key = lowercase(trimmed);
    list.erase(std::remove_if(list.begin(), list.end(), [&key](const std::string& e) { return lowercase(e) == key; }),
        list.end());
    list.insert(list.begin(), trimmed);
    if (list.size() > MAX_HISTORY_ENTRIES)
        list.resize(MAX_HISTORY_ENTRIES);
    return true;
}

bool SearchHistory::remove(size_t index)
{
    if (index >= list.size())
        return false;
    list.erase(list.begin() + index);
    return true;
}

bool SearchHistory::clear()
{
    bool changed = !list.empty();
    list.clear();
    return changed;
}

nlohmann::json SearchHistory::toJson() const
{
    return list;
}

SearchHistory SearchHistory::fromJson(const nlohmann::json& json)
{
    SearchHistory history;
    if (!json.is_array())
        return history;
    // stored newest first, so replaying oldest first keeps that order
    for (auto it = json.rbegin(); it != json.rend(); ++it)
        if (it->is_string())
            history.add(it->get<std::string>());
    return history;
}

} // namespace nuvio::data::search
