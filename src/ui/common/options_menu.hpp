#pragma once

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nuvio::ui
{

// A borealis dropdown of options. onChosen runs with the picked index once the menu has closed
// and focus is back where it was, so it can rebuild the views the focus returns to.
// Cancelling (B) calls nothing.
inline void showOptions(const std::string& title, const std::vector<std::string>& options, int selected,
    std::function<void(int)> onChosen)
{
    auto chosen    = std::make_shared<int>(-1);
    auto* dropdown = new brls::Dropdown(
        title, options, [chosen](int index) { *chosen = index; }, selected,
        [chosen, onChosen = std::move(onChosen)](int)
        {
            if (*chosen >= 0 && onChosen)
                onChosen(*chosen);
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

} // namespace nuvio::ui
