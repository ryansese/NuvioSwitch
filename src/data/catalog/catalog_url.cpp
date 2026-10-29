#include "data/catalog/catalog_url.hpp"

#include <cctype>

namespace nuvio::data::catalog
{

namespace
{
std::string trimTrailingSlashes(std::string s)
{
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}
} // namespace

std::string encodeArg(const std::string& value)
{
    static const char* const HEX = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value)
    {
        // the characters URLEncoder leaves alone
        if (std::isalnum(c) || c == '.' || c == '-' || c == '*' || c == '_')
            out += static_cast<char>(c);
        else if (c == ' ')
            out += "%20";
        else
        {
            out += '%';
            out += HEX[c >> 4];
            out += HEX[c & 0xF];
        }
    }
    return out;
}

std::string buildCatalogUrl(const std::string& baseUrl, const std::string& type, const std::string& catalogId,
    int skip, const Extras& extras)
{
    std::string trimmed = trimTrailingSlashes(baseUrl);
    size_t queryStart   = trimmed.find('?');
    std::string basePath =
        queryStart == std::string::npos ? trimmed : trimTrailingSlashes(trimmed.substr(0, queryStart));
    std::string baseQuery = queryStart == std::string::npos ? "" : trimmed.substr(queryStart);

    std::string path = basePath + "/catalog/" + type + "/" + catalogId;
    if (extras.empty())
    {
        if (skip > 0)
            path += "/skip=" + std::to_string(skip);
    }
    else
    {
        // LinkedHashMap semantics: a repeated key keeps its first position and last value
        Extras args;
        for (const auto& [key, value] : extras)
        {
            auto it = args.begin();
            while (it != args.end() && it->first != key)
                ++it;
            if (it != args.end())
                it->second = value;
            else
                args.emplace_back(key, value);
        }
        bool hasSkip = false;
        for (const auto& arg : args)
            hasSkip = hasSkip || arg.first == "skip";
        if (!hasSkip && skip > 0)
            args.emplace_back("skip", std::to_string(skip));

        path += "/";
        for (size_t i = 0; i < args.size(); i++)
            path += (i ? "&" : "") + encodeArg(args[i].first) + "=" + encodeArg(args[i].second);
    }
    return path + ".json" + baseQuery;
}

} // namespace nuvio::data::catalog
