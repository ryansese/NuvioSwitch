#pragma once

#include <borealis.hpp>
#include <vector>

// Temporary: five Cinemeta posters to exercise ImageLoader (NuvioSwitch-0002).
// Only added to Home when built with -DNUVIO_DEBUG_NETWORK=ON.
// A on a poster rotates the URLs across the cells, reloading every cell at once
// to exercise stale-load handling and the memory cache.
class DebugPosterRow : public brls::Box
{
  public:
    DebugPosterRow();
    ~DebugPosterRow() override;

  private:
    void loadAll();

    std::vector<brls::Image*> posters;
    size_t offset = 0;
};
