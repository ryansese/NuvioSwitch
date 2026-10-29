#pragma once

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <unistd.h>

// A fresh directory under the system temp folder, deleted with its contents at scope end.
// path() ends with '/'.
class TempDir
{
  public:
    TempDir()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "nuvioswitch-test-XXXXXX").string();
        char* created       = mkdtemp(pattern.data());
        dir                 = std::string(created ? created : pattern.c_str()) + "/";
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    const std::string& path() const { return dir; }

  private:
    std::string dir;
};

// Sets an environment variable for the current scope, restoring the old value after.
class ScopedEnv
{
  public:
    ScopedEnv(const char* name, const std::string& value) : name(name)
    {
        if (const char* old = std::getenv(name))
            previous = old;
        setenv(name, value.c_str(), 1);
    }
    ~ScopedEnv()
    {
        if (previous)
            setenv(name, previous->c_str(), 1);
        else
            unsetenv(name);
    }

  private:
    const char* name;
    std::optional<std::string> previous;
};
