#include "core/cache/lru_index.hpp"

namespace nuvio::core::cache
{

std::vector<std::string> LruIndex::put(const std::string& key, uint64_t size)
{
    remove(key);
    order.push_front(key);
    entries[key] = Entry{size, order.begin()};
    total += size;

    std::vector<std::string> evicted;
    while (total > maxBytes && !order.empty())
    {
        std::string oldest = order.back();
        evicted.push_back(oldest);
        remove(oldest);
    }
    return evicted;
}

bool LruIndex::touch(const std::string& key)
{
    auto it = entries.find(key);
    if (it == entries.end())
        return false;
    order.splice(order.begin(), order, it->second.position);
    return true;
}

void LruIndex::remove(const std::string& key)
{
    auto it = entries.find(key);
    if (it == entries.end())
        return;
    total -= it->second.size;
    order.erase(it->second.position);
    entries.erase(it);
}

std::vector<std::string> LruIndex::keysOldestFirst() const
{
    return std::vector<std::string>(order.rbegin(), order.rend());
}

} // namespace nuvio::core::cache
