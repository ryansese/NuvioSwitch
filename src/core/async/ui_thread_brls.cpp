#include "core/async/ui_thread.hpp"

#include <borealis/core/thread.hpp>

namespace nuvio::core::async
{

void postToUi(std::function<void()> fn)
{
    brls::sync(fn);
}

} // namespace nuvio::core::async
