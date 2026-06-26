// SPDX-FileCopyrightText: Fondazione Istituto Italiano di Tecnologia (IIT)
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RESOLVE_ROBOTICS_URI_CPP_H
#define RESOLVE_ROBOTICS_URI_CPP_H

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ResolveRoboticsURICpp
{

struct ResolveRoboticsURIOptions
{
    // Disable searching inside the active environment prefix (<prefix>/share).
    bool excludeActivePrefix = false;

    // Exclude specific env vars from default search path extraction.
    std::unordered_set<std::string> excludeEnvVars;

    // Additional directories to search as-is.
    std::vector<std::string> packageDirs;
};

inline std::string cleanPathSeparator(const std::string& filename, const bool isWindows)
{
    std::string output = filename;
    char pathSeparator = isWindows ? '\\' : '/';
    char wrongPathSeparator = isWindows ? '/' : '\\';
    for (size_t i = 0; i < output.size(); ++i)
    {
        if (output[i] == wrongPathSeparator)
        {
            output[i] = pathSeparator;
        }
    }
    return output;
}

inline bool isFileExisting(const std::string& filename)
{
    if (FILE* file = fopen(filename.c_str(), "r"))
    {
        fclose(file);
        return true;
    } else
    {
        return false;
    }
}

inline std::string joinPaths(const std::string& base,
                             const std::string& suffix,
                             const bool isWindows)
{
    if (base.empty())
    {
        return suffix;
    }

    if (suffix.empty())
    {
        return base;
    }

    const char pathSeparator = isWindows ? '\\' : '/';
    const bool baseHasSeparator = base.back() == '/' || base.back() == '\\';
    const bool suffixHasSeparator = suffix.front() == '/' || suffix.front() == '\\';

    if (baseHasSeparator && suffixHasSeparator)
    {
        return base + suffix.substr(1);
    }

    if (!baseHasSeparator && !suffixHasSeparator)
    {
        return base + pathSeparator + suffix;
    }

    return base + suffix;
}

inline bool getFilePath(const std::string& filename,
                        const std::string& prefixToRemove,
                        const std::unordered_set<std::string>& paths,
                        const bool isWindows,
                        std::string& outputFileName)
{
    if (filename.substr(0, prefixToRemove.size()) != prefixToRemove)
    {
        return false;
    }

    std::string filenameNoPrefix = filename;
    filenameNoPrefix.erase(0, prefixToRemove.size());

    for (const std::string& path : paths)
    {
        const std::string testPath =
            cleanPathSeparator(joinPaths(path, filenameNoPrefix, isWindows), isWindows);
        if (isFileExisting(testPath))
        {
            outputFileName = testPath;
            return true;
        }
    }

    return false;
}

inline std::unordered_map<std::string, bool> getSupportedEnvVars()
{
    // true means the variable contains prefixes and we append /share,
    // false means we search the provided directories as-is.
    return {{"ROS_PACKAGE_PATH", false},
            {"GAZEBO_MODEL_PATH", false},
            {"SDF_PATH", false},
            {"IGN_GAZEBO_RESOURCE_PATH", false},
            {"GZ_SIM_RESOURCE_PATH", false},
            {"RRU_ADDITIONAL_PATHS", false},
            {"AMENT_PREFIX_PATH", true}};
}

inline bool isEnvVarExcluded(const std::string& envVarName,
                             const ResolveRoboticsURIOptions& options)
{
    return options.excludeEnvVars.find(envVarName) != options.excludeEnvVars.end();
}

inline std::optional<std::string> getActivePrefixPath()
{
    const char* condaPrefix = std::getenv("CONDA_PREFIX");
    if (condaPrefix && std::string(condaPrefix).size() > 0)
    {
        return std::string(condaPrefix);
    }

    const char* virtualEnv = std::getenv("VIRTUAL_ENV");
    if (virtualEnv && std::string(virtualEnv).size() > 0)
    {
        return std::string(virtualEnv);
    }

    return {};
}

inline std::optional<std::string>
resolveRoboticsURI(const std::string& uriFilename,
                   const ResolveRoboticsURIOptions& options,
                   std::string& errorMessage)
{
    bool isWindows = false;
#ifdef _WIN32
    isWindows = true;
#endif

    // If file starts with file:/, remove file:/ and return if it exists
    std::string fileUriPrefix = "file:/";
    if (uriFilename.substr(0, fileUriPrefix.size()) == fileUriPrefix)
    {
        std::string uriFilename_noprefix = uriFilename;
        uriFilename_noprefix.erase(0, fileUriPrefix.size());

        if (isFileExisting(uriFilename_noprefix))
        {
            return uriFilename_noprefix;
        }
    }

    // If the file exists with removing any prefix, just return it
    if (isFileExisting(uriFilename))
    {
        return uriFilename;
    }

    const std::string packageUriPrefix = "package:/";
    const std::string modelUriPrefix = "model:/";

    if (uriFilename.substr(0, packageUriPrefix.size()) != packageUriPrefix
        && uriFilename.substr(0, modelUriPrefix.size()) != modelUriPrefix)
    {
        // The uri does not start with file:/ (earlier check, is not a file path
        // or start with model:/ or package:/, we can't resolve it
        errorMessage = "ResolveRoboticsURICpp: URI " + uriFilename
                       + " is not a existing file path and does not start with file:/, package:/ "
                         "or model:/, it is not possible to resolve it.";
        return {};
    }

    std::string uriPrefix;
    if (uriFilename.substr(0, packageUriPrefix.size()) == packageUriPrefix)
    {
        uriPrefix = packageUriPrefix;
    }

    if (uriFilename.substr(0, modelUriPrefix.size()) == modelUriPrefix)
    {
        uriPrefix = modelUriPrefix;
    }

    // Actually solve it

    // At this point, let's process the actual case of package:/ or model:/ URIs
    std::unordered_set<std::string> pathList;

    // Extract list of directories where to search for the file
    const auto supportedEnvVars = getSupportedEnvVars();
    for (const auto& envSpec : supportedEnvVars)
    {
        const std::string& envVarName = envSpec.first;
        const bool isPrefixEnvVar = envSpec.second;

        if (isEnvVarExcluded(envVarName, options))
        {
            continue;
        }

        const char* envVarValue = std::getenv(envVarName.c_str());
        if (!envVarValue)
        {
            continue;
        }

        std::stringstream envVarString(envVarValue);
        std::string individualPath;
        while (std::getline(envVarString, individualPath, isWindows ? ';' : ':'))
        {
            if (individualPath.empty())
            {
                continue;
            }

            if (isPrefixEnvVar)
            {
                pathList.insert(individualPath + "/share");
            } else
            {
                pathList.insert(individualPath);
            }
        }
    }

    if (!options.excludeActivePrefix)
    {
        const std::optional<std::string> activePrefix = getActivePrefixPath();
        if (activePrefix.has_value())
        {
            pathList.insert(activePrefix.value() + "/share");
#ifdef _WIN32
            pathList.insert(activePrefix.value() + "/Library/share");
#endif
        }
    }

    for (const std::string& packageDir : options.packageDirs)
    {
        if (!packageDir.empty())
        {
            pathList.insert(packageDir);
        }
    }

    std::string realAbsoluteFileName;
    bool ok = getFilePath(uriFilename, uriPrefix, pathList, isWindows, realAbsoluteFileName);

    if (ok)
    {
        return realAbsoluteFileName;
    } else
    {
        errorMessage = "Impossible to resolve uri " + uriFilename;
        return {};
    }
}

inline std::optional<std::string>
resolveRoboticsURI(const std::string& uriFilename, std::string& errorMessage)
{
    ResolveRoboticsURIOptions defaultOptions;
    return resolveRoboticsURI(uriFilename, defaultOptions, errorMessage);
}

inline std::optional<std::string>
resolveRoboticsURI(const std::string& uriFilename, const ResolveRoboticsURIOptions& options)
{
    std::string dummyErrorMessage;
    return resolveRoboticsURI(uriFilename, options, dummyErrorMessage);
}

inline std::optional<std::string> resolveRoboticsURI(const std::string& uriFilename)
{
    std::string dummyErrorMessage;
    ResolveRoboticsURIOptions defaultOptions;
    return resolveRoboticsURI(uriFilename, defaultOptions, dummyErrorMessage);
}

} // namespace ResolveRoboticsURICpp

#endif