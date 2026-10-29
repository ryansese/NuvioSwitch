#pragma once

#include <functional>

namespace nuvio::core::async
{

// Runs fn on the UI thread before the next frame (brls::sync).
// Implemented in ui_thread_brls.cpp, so it is not part of the test target.
void postToUi(std::function<void()> fn);

} // namespace nuvio::core::async
