#pragma once

namespace nuvio::core::storage
{

// The active profile, for profile-scoped stores.
// Fixed at 1 until profiles land (NuvioSwitch-0013).
class ProfileContext
{
  public:
    static int current() { return 1; }
};

} // namespace nuvio::core::storage
