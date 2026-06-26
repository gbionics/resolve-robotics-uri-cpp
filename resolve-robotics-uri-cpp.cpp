// SPDX-FileCopyrightText: Fondazione Istituto Italiano di Tecnologia (IIT)
// SPDX-License-Identifier: BSD-3-Clause

#include <ResolveRoboticsURICpp.h>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <unordered_set>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "resolve-robotics-uri-cpp: Wrong number of parameters. Usage " << argv[0]
                  << " [--exclude-active-prefix] [--exclude-env-var NAME]"
                     " [--package-dir PATH] URI"
                  << std::endl;
        return EXIT_FAILURE;
    }

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    std::string uri;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--exclude-active-prefix")
        {
            options.excludeActivePrefix = true;
            continue;
        }

        if (arg == "--exclude-env-var")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "resolve-robotics-uri-cpp: Missing argument for --exclude-env-var"
                          << std::endl;
                return EXIT_FAILURE;
            }

            options.excludeEnvVars.insert(argv[++i]);
            continue;
        }

        if (arg == "--package-dir")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "resolve-robotics-uri-cpp: Missing argument for --package-dir"
                          << std::endl;
                return EXIT_FAILURE;
            }

            options.packageDirs.push_back(argv[++i]);
            continue;
        }

        if (!uri.empty())
        {
            std::cerr << "resolve-robotics-uri-cpp: Multiple URI arguments passed." << std::endl;
            return EXIT_FAILURE;
        }

        uri = arg;
    }

    if (uri.empty())
    {
        std::cerr << "resolve-robotics-uri-cpp: Missing URI argument." << std::endl;
        return EXIT_FAILURE;
    }

    std::string errorMessage;
    auto absolute_file_name = ResolveRoboticsURICpp::resolveRoboticsURI(uri, options, errorMessage);

    if (absolute_file_name.has_value())
    {
        std::cout << absolute_file_name.value();
    } else
    {
        std::cerr << "resolve-robotics-uri-cpp: Impossible to find URI " << argv[1] << ", "
                  << errorMessage << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
