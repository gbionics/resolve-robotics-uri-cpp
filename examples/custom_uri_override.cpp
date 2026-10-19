// SPDX-License-Identifier: BSD-3-Clause
// Usage: custom-uri-override CONFIG_ROOT config://controller.ini

#include <ResolveRoboticsURICpp.h>
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: custom-uri-override CONFIG_ROOT URI\n";
        return 1;
    }

    const auto configRoot = std::filesystem::absolute(argv[1]);
    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    // Resolve config:// URIs relative to an application-specific directory.
    options.uri_override["config://"]
        = [configRoot](const std::string& uri,
                       const ResolveRoboticsURICpp::ResolveRoboticsURIOptions& opts,
                       std::string& error) {
              const auto path = configRoot / uri.substr(std::string("config://").size());
              auto nested = opts;
              nested.uri_override.clear();
              return ResolveRoboticsURICpp::resolveRoboticsURI(path.string(), nested, error);
          };

    std::string error;
    const auto path = ResolveRoboticsURICpp::resolveRoboticsURI(argv[2], options, error);
    if (!path)
    {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << *path << '\n';
    return 0;
}
