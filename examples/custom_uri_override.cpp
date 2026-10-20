// SPDX-License-Identifier: BSD-3-Clause
// Usage: custom-uri-override home://Documents/notes.txt

#include <ResolveRoboticsURICpp.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: custom-uri-override URI\n";
        return 1;
    }

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    // Resolve home://Documents/notes.txt inside the current user's home directory.
    options.uriOverrides["home"] = [](const std::string& uri,
                                      const ResolveRoboticsURICpp::ResolveRoboticsURIOptions& opts,
                                      std::string& error) -> std::optional<std::string> {
#ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
#else
        const char* home = std::getenv("HOME");
#endif
        if (!home || !*home)
        {
            error = "User home directory is not set";
            return {};
        }
        auto relative = uri.substr(uri.find(':') + 1);
        if (relative.rfind("//", 0) == 0)
        {
            relative.erase(0, 2);
        }
        const auto path = std::filesystem::path(home) / relative;
        auto nested = opts;
        nested.uriOverrides.clear();
        return ResolveRoboticsURICpp::resolveRoboticsURI(path.string(), nested, error);
    };

    std::string error;
    const auto path = ResolveRoboticsURICpp::resolveRoboticsURI(argv[1], options, error);
    if (!path)
    {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << *path << '\n';
    return 0;
}
