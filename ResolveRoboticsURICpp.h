// SPDX-FileCopyrightText: Fondazione Istituto Italiano di Tecnologia (IIT)
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RESOLVE_ROBOTICS_URI_CPP_H
#define RESOLVE_ROBOTICS_URI_CPP_H

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ResolveRoboticsURICpp
{

struct ResolveRoboticsURIOptions;
namespace detail
{
inline std::optional<std::string> resolveRobotURI(const std::string& uri,
                                                  const ResolveRoboticsURIOptions& options,
                                                  std::string& errorMessage);
}

using URIResolver = std::function<
    std::optional<std::string>(const std::string&, const ResolveRoboticsURIOptions&, std::string&)>;

struct ResolveRoboticsURIOptions
{
    // Disable searching inside the active environment prefix (<prefix>/share).
    bool excludeActivePrefix = false;

    // Exclude specific env vars from default search path extraction.
    std::unordered_set<std::string> excludeEnvVars;

    // Additional directories to search as-is.
    std::vector<std::string> packageDirs;

    // Optional default package for robot://, also available to other URI overrides.
    std::string defaultPackageName;

    // Longest matching prefix wins; callbacks receive the full URI and options.
    // Clear this map to disable overrides. A selected callback owns failures too.
    std::map<std::string, URIResolver> uriOverrides{{"robot://", detail::resolveRobotURI}};
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

inline bool isPathExisting(const std::string& filename)
{
    std::error_code ec;
    return std::filesystem::exists(filename, ec);
}

inline std::string fileUriToLocalPath(const std::string& uriFilename, const bool isWindows)
{
    const std::string fileScheme = "file:";
    if (uriFilename.substr(0, fileScheme.size()) != fileScheme)
    {
        return uriFilename;
    }

    std::string localPath = uriFilename.substr(fileScheme.size());

    if (isWindows)
    {
        while (!localPath.empty() && (localPath.front() == '/' || localPath.front() == '\\'))
        {
            localPath.erase(0, 1);
        }

        return cleanPathSeparator(localPath, isWindows);
    }

    std::size_t leadingSlashCount = 0;
    while (leadingSlashCount < localPath.size() && localPath[leadingSlashCount] == '/')
    {
        ++leadingSlashCount;
    }

    if (leadingSlashCount > 1)
    {
        localPath.erase(0, leadingSlashCount - 1);
    }

    return localPath;
}

inline std::string
joinPaths(const std::string& base, const std::string& suffix, const bool isWindows)
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
        const std::string testPath
            = cleanPathSeparator(joinPaths(path, filenameNoPrefix, isWindows), isWindows);
        if (isPathExisting(testPath))
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

inline bool
isEnvVarExcluded(const std::string& envVarName, const ResolveRoboticsURIOptions& options)
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

inline std::unordered_set<std::string> getSearchPaths(const ResolveRoboticsURIOptions& options)
{
    bool isWindows = false;
#ifdef _WIN32
    isWindows = true;
#endif
    // Collect package roots shared by URI and robot-specific resolution.
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

    return pathList;
}

inline std::optional<std::string> resolveRoboticsURI(const std::string& uriFilename,
                                                     const ResolveRoboticsURIOptions& options,
                                                     std::string& errorMessage)
{
    bool isWindows = false;
#ifdef _WIN32
    isWindows = true;
#endif

    errorMessage.clear();
    const URIResolver* resolver = nullptr;
    std::size_t prefixLength = 0;
    for (const auto& entry : options.uriOverrides)
    {
        if (entry.first.empty() || !entry.second)
        {
            errorMessage = "URI overrides require nonempty prefixes and callbacks";
            return {};
        }
        if (entry.first.size() > prefixLength && uriFilename.rfind(entry.first, 0) == 0)
        {
            resolver = &entry.second;
            prefixLength = entry.first.size();
        }
    }
    if (resolver)
    {
        return (*resolver)(uriFilename, options, errorMessage);
    }

    // If path starts with file:, convert to local path and return if it exists.
    const std::string fileScheme = "file:";
    if (uriFilename.substr(0, fileScheme.size()) == fileScheme)
    {
        const std::string uriFilename_noprefix = fileUriToLocalPath(uriFilename, isWindows);

        if (isPathExisting(uriFilename_noprefix))
        {
            return uriFilename_noprefix;
        }
    }

    // If the file exists with removing any prefix, just return it
    if (isPathExisting(uriFilename))
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

    const auto pathList = getSearchPaths(options);

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

namespace detail
{
struct RobotDirectoryCache
{
    std::mutex mutex;
    std::map<std::vector<std::filesystem::path>, std::vector<std::filesystem::path>> directories;
};

inline RobotDirectoryCache& robotDirectoryCache()
{
    static RobotDirectoryCache cache;
    return cache;
}

inline std::vector<std::filesystem::path>
robotDirectories(const std::vector<std::filesystem::path>& roots)
{
    auto& cache = robotDirectoryCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    const auto found = cache.directories.find(roots);
    if (found != cache.directories.end())
    {
        return found->second;
    }
    std::vector<std::filesystem::path> result;
    for (const auto& root : roots)
    {
        std::error_code ec;
        std::filesystem::directory_iterator it(root, ec), end;
        while (!ec && it != end)
        {
            const auto robots = it->path() / "robots";
            if (it->is_directory(ec) && std::filesystem::is_directory(robots, ec))
            {
                const auto canonical = std::filesystem::canonical(robots, ec);
                if (!ec)
                {
                    result.push_back(canonical);
                }
            }
            ec.clear();
            it.increment(ec);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    if (cache.directories.size() >= 128)
    {
        cache.directories.clear();
    }
    cache.directories.emplace(roots, result);
    return result;
}
} // namespace detail

/// Refresh discovery after installing new packages or robots directories.
inline void clearRobotSpecificFileCache()
{
    auto& cache = detail::robotDirectoryCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    cache.directories.clear();
}

namespace detail
{
inline std::optional<std::string> resolveRobotURI(const std::string& uri,
                                                  const ResolveRoboticsURIOptions& options,
                                                  std::string& errorMessage)
{
    const std::string relativePath = uri.substr(std::string("robot://").size());
    const std::string& defaultPackageName = options.defaultPackageName;
    const std::filesystem::path path(relativePath);
    const auto validName = [](const std::string& name) {
        return !name.empty() && name != "." && name != ".."
               && name.find_first_of("/\\:") == std::string::npos;
    };
    if (relativePath.empty() || path.has_root_path()
        || relativePath.substr(0, relativePath.find_first_of("/\\")).find(':') != std::string::npos
        || std::find(path.begin(), path.end(), std::filesystem::path("..")) != path.end()
        || (!defaultPackageName.empty() && !validName(defaultPackageName)))
    {
        errorMessage = "Invalid relative path or package name for robot:// URI";
        return {};
    }
    const auto regularFile
        = [](const std::filesystem::path& candidate) -> std::optional<std::string> {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec))
        {
            auto canonical = std::filesystem::canonical(candidate, ec);
            if (!ec)
            {
                return canonical.string();
            }
        }
        return {};
    };
    if (auto local = regularFile(path))
    {
        return local;
    }
    const char* robotEnv = std::getenv("ACTIVE_ROBOT_NAME");
    if (!robotEnv || !*robotEnv)
    {
        robotEnv = std::getenv("YARP_ROBOT_NAME");
    }
    const std::string robot = robotEnv ? robotEnv : "";
    if (!robot.empty() && !validName(robot))
    {
        errorMessage = "Robot name must be a single directory component";
        return {};
    }
    std::vector<std::filesystem::path> roots;
    for (const auto& root : getSearchPaths(options))
    {
        std::error_code ec;
        if (std::filesystem::is_directory(root, ec))
        {
            auto canonical = std::filesystem::canonical(root, ec);
            if (!ec)
            {
                roots.push_back(canonical);
            }
        }
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
    const auto find = [&](const std::vector<std::filesystem::path>& bases,
                          const std::filesystem::path& suffix) -> std::optional<std::string> {
        std::vector<std::string> matches;
        for (const auto& base : bases)
        {
            if (auto match = regularFile(base / suffix))
            {
                matches.push_back(*match);
            }
        }
        if (matches.empty())
        {
            return {};
        }
        return *std::min_element(matches.begin(), matches.end());
    };
    if (!robot.empty())
    {
        if (auto overrideFile
            = find(detail::robotDirectories(roots), std::filesystem::path(robot) / path))
        {
            return overrideFile;
        }
    }
    if (!defaultPackageName.empty())
    {
        if (auto defaultFile = find(roots, std::filesystem::path(defaultPackageName) / path))
        {
            return defaultFile;
        }
    }
    errorMessage = "No robot-specific or default file '" + relativePath + "' found for package '"
                   + defaultPackageName + "'";
    return {};
}

} // namespace detail

} // namespace ResolveRoboticsURICpp

#endif
