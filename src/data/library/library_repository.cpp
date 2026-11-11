#include "data/library/library_repository.hpp"

#include <algorithm>
#include <chrono>

namespace nuvio::data::library
{

namespace
{
LibraryRepository* sharedRepository = nullptr;
}

LibraryRepository::LibraryRepository(Config value) : config(std::move(value))
{
    if (!config.nowMs)
        config.nowMs = []
        {
            using namespace std::chrono;
            return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        };
    if (!config.store)
        return;

    auto stored = config.store->get("items");
    if (stored.is_array())
        for (const auto& json : stored)
            if (auto item = itemFromJson(json); item && !contains(item->id, item->type))
                list.push_back(std::move(*item));
    sortOption   = sortFromKey(config.store->get<std::string>("sort", ""));
    filterOption = filterFromKey(config.store->get<std::string>("filter", ""));
}

bool LibraryRepository::contains(const std::string& id, const std::string& type) const
{
    return std::any_of(list.begin(), list.end(), [&](const auto& item) { return item.id == id && item.type == type; });
}

bool LibraryRepository::add(SavedLibraryItem item)
{
    if (item.id.empty() || contains(item.id, item.type))
        return false;
    if (item.addedAt <= 0)
        item.addedAt = now();
    list.push_back(std::move(item));
    write();
    notify();
    return true;
}

bool LibraryRepository::remove(const std::string& id, const std::string& type)
{
    auto before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& item) { return item.id == id && item.type == type; }),
        list.end());
    if (list.size() == before)
        return false;
    write();
    notify();
    return true;
}

void LibraryRepository::setSort(Sort sort)
{
    if (sort == sortOption)
        return;
    sortOption = sort;
    write();
    notify();
}

void LibraryRepository::setFilter(Filter filter)
{
    if (filter == filterOption)
        return;
    filterOption = filter;
    write();
    notify();
}

void LibraryRepository::write()
{
    if (!config.store)
        return;
    auto items = nlohmann::json::array();
    for (const auto& item : list)
        items.push_back(toJson(item));
    config.store->set("items", std::move(items));
    config.store->set("sort", sortKey(sortOption));
    config.store->set("filter", filterKey(filterOption));
    config.store->save();
}

int LibraryRepository::subscribe(Listener listener)
{
    int token = nextToken++;
    listeners[token] = std::move(listener);
    return token;
}

void LibraryRepository::unsubscribe(int token)
{
    listeners.erase(token);
}

void LibraryRepository::notify()
{
    auto copy = listeners;
    for (auto& [token, listener] : copy)
        if (listeners.count(token))
            listener();
}

LibraryRepository& LibraryRepository::shared()
{
    return *sharedRepository;
}

void LibraryRepository::setShared(LibraryRepository* repository)
{
    sharedRepository = repository;
}

bool LibraryRepository::hasShared()
{
    return sharedRepository != nullptr;
}

} // namespace nuvio::data::library
