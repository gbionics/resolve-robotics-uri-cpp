// SPDX-FileCopyrightText: Fondazione Istituto Italiano di Tecnologia (IIT)
// SPDX-License-Identifier: BSD-3-Clause

#include <catch2/catch_test_macros.hpp>

#include <ResolveRoboticsURICpp.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{

class ScopedEnvVar
{
public:
    explicit ScopedEnvVar(const std::string& name)
        : m_name(name)
    {
        const char* existing = std::getenv(m_name.c_str());
        if (existing)
        {
            m_hadOriginalValue = true;
            m_originalValue = existing;
        }
    }

    ~ScopedEnvVar()
    {
        if (m_hadOriginalValue)
        {
            set(m_originalValue);
        } else
        {
            unset();
        }
    }

    void set(const std::string& value)
    {
#ifdef _WIN32
        _putenv_s(m_name.c_str(), value.c_str());
#else
        setenv(m_name.c_str(), value.c_str(), 1);
#endif
    }

    void unset()
    {
#ifdef _WIN32
        _putenv_s(m_name.c_str(), "");
#else
        unsetenv(m_name.c_str());
#endif
    }

private:
    std::string m_name;
    bool m_hadOriginalValue = false;
    std::string m_originalValue;
};

std::filesystem::path
writeFile(const std::filesystem::path& filePath, const std::string& content = "test")
{
    std::filesystem::create_directories(filePath.parent_path());
    std::ofstream output(filePath);
    output << content;
    return std::filesystem::path(filePath).make_preferred();
}

std::filesystem::path writeDirectory(const std::filesystem::path& directoryPath)
{
    std::filesystem::create_directories(directoryPath);
    return std::filesystem::path(directoryPath).make_preferred();
}

std::string fileUriFromPath(const std::filesystem::path& path)
{
    const std::string genericPath = path.generic_string();
    if (!genericPath.empty() && genericPath.front() == '/')
    {
        return "file://" + genericPath;
    }

    return "file:///" + genericPath;
}

std::vector<std::string> supportedEnvVars()
{
    return {"ROS_PACKAGE_PATH",
            "GAZEBO_MODEL_PATH",
            "SDF_PATH",
            "IGN_GAZEBO_RESOURCE_PATH",
            "GZ_SIM_RESOURCE_PATH",
            "RRU_ADDITIONAL_PATHS",
            "AMENT_PREFIX_PATH",
            "CONDA_PREFIX",
            "VIRTUAL_ENV"};
}

} // namespace

TEST_CASE("FileDoesNotExist")
{
    CHECK_FALSE(ResolveRoboticsURICpp::resolveRoboticsURI("package://this/package/and/file/does/"
                                                          "not.exist")
                    .has_value());
}

TEST_CASE("ResolveFromCondaPrefixAndOptOut")
{
    std::vector<std::unique_ptr<ScopedEnvVar>> scopedEnvVars;
    scopedEnvVars.reserve(supportedEnvVars().size());
    for (const auto& envVar : supportedEnvVars())
    {
        scopedEnvVars.emplace_back(new ScopedEnvVar(envVar));
        scopedEnvVars.back()->unset();
    }

    ScopedEnvVar condaPrefix("CONDA_PREFIX");
    condaPrefix.unset();

    const std::filesystem::path prefixPath
        = std::filesystem::temp_directory_path() / "rru_cpp_conda_prefix_test";
    const std::filesystem::path cubeUrdf
        = writeFile(prefixPath / "share" / "example_cpp_package" / "cube.urdf", "<robot/>");

    condaPrefix.set(prefixPath.string());

    const std::string uri = "package://example_cpp_package/cube.urdf";
    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI(uri);
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == cubeUrdf.string());

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    options.excludeActivePrefix = true;
    CHECK_FALSE(ResolveRoboticsURICpp::resolveRoboticsURI(uri, options).has_value());

    std::filesystem::remove_all(prefixPath);
}

TEST_CASE("ResolveFromVirtualEnvPrefixFallback")
{
    std::vector<std::unique_ptr<ScopedEnvVar>> scopedEnvVars;
    scopedEnvVars.reserve(supportedEnvVars().size());
    for (const auto& envVar : supportedEnvVars())
    {
        scopedEnvVars.emplace_back(new ScopedEnvVar(envVar));
        scopedEnvVars.back()->unset();
    }

    ScopedEnvVar virtualEnv("VIRTUAL_ENV");
    virtualEnv.unset();

    const std::filesystem::path prefixPath
        = std::filesystem::temp_directory_path() / "rru_cpp_virtualenv_prefix_test";
    const std::filesystem::path cubeUrdf
        = writeFile(prefixPath / "share" / "example_cpp_package" / "cube.urdf", "<robot/>");

    virtualEnv.set(prefixPath.string());

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI("package://example_cpp_package/"
                                                              "cube.urdf");
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == cubeUrdf.string());

    std::filesystem::remove_all(prefixPath);
}

TEST_CASE("ExcludeEnvVar")
{
    ScopedEnvVar gazeboModelPath("GAZEBO_MODEL_PATH");
    gazeboModelPath.unset();

    const std::filesystem::path modelDir
        = std::filesystem::temp_directory_path() / "rru_cpp_gazebo_model_path_test";
    const std::filesystem::path modelFile = writeFile(modelDir / "example_model");

    gazeboModelPath.set(modelDir.string());

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI("model://example_model");
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == modelFile.string());

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    options.excludeEnvVars.insert("GAZEBO_MODEL_PATH");
    CHECK_FALSE(
        ResolveRoboticsURICpp::resolveRoboticsURI("model://example_model", options).has_value());

    std::filesystem::remove_all(modelDir);
}

TEST_CASE("AdditionalPackageDirs")
{
    const std::filesystem::path additionalDir
        = std::filesystem::temp_directory_path() / "rru_cpp_package_dirs_test";
    const std::filesystem::path cubeUrdf
        = writeFile(additionalDir / "example_cpp_package" / "cube.urdf", "<robot/>");

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    options.excludeActivePrefix = true;
    options.packageDirs.push_back(additionalDir.string());

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI("package://example_cpp_package/"
                                                              "cube.urdf",
                                                              options);
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == cubeUrdf.string());

    std::filesystem::remove_all(additionalDir);
}

TEST_CASE("ResolveExistingDirectoryPath")
{
    const std::filesystem::path directoryPath = writeDirectory(
        std::filesystem::temp_directory_path() / "rru_cpp_existing_dir_test" / "nested_dir");

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI(directoryPath.string());
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == directoryPath.string());

    std::filesystem::remove_all(std::filesystem::temp_directory_path()
                                / "rru_cpp_existing_dir_test");
}

TEST_CASE("ResolveExistingDirectoryFileUri")
{
    const std::filesystem::path directoryPath = writeDirectory(
        std::filesystem::temp_directory_path() / "rru_cpp_file_uri_dir_test" / "nested_dir");

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI(fileUriFromPath(directoryPath));
    REQUIRE(resolved.has_value());
    CHECK(std::filesystem::equivalent(std::filesystem::path(resolved.value()), directoryPath));

    std::filesystem::remove_all(std::filesystem::temp_directory_path()
                                / "rru_cpp_file_uri_dir_test");
}

TEST_CASE("ResolveDirectoryFromPackageUri")
{
    const std::filesystem::path additionalDir
        = std::filesystem::temp_directory_path() / "rru_cpp_package_dir_uri_test";
    const std::filesystem::path packageDirectory
        = writeDirectory(additionalDir / "example_cpp_package" / "meshes");

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    options.excludeActivePrefix = true;
    options.packageDirs.push_back(additionalDir.string());

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI("package://example_cpp_package/"
                                                              "meshes",
                                                              options);
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == packageDirectory.string());

    std::filesystem::remove_all(additionalDir);
}

TEST_CASE("RobotSpecificFile")
{
    using namespace ResolveRoboticsURICpp;
    std::vector<std::unique_ptr<ScopedEnvVar>> envs;
    for (const auto& name : supportedEnvVars())
    {
        envs.emplace_back(new ScopedEnvVar(name));
        envs.back()->unset();
    }
    ScopedEnvVar active("ACTIVE_ROBOT_NAME"), yarp("YARP_ROBOT_NAME");
    active.unset();
    yarp.unset();
    const auto base = std::filesystem::canonical(std::filesystem::temp_directory_path())
                      / "rru_cpp_robot_specific_test";
    struct Cleanup
    {
        std::filesystem::path base, cwd = std::filesystem::current_path();
        ~Cleanup()
        {
            std::filesystem::current_path(cwd);
            std::filesystem::remove_all(base);
            clearRobotSpecificFileCache();
        }
    } cleanup{base};
    std::filesystem::remove_all(base);
    const auto cwd = writeDirectory(base / "cwd");
    std::filesystem::current_path(cwd);
    const auto root = base / "share";
    const auto fallback = writeFile(root / "ControlLib/config/controller.ini");
    const auto overrideFile
        = writeFile(root / "CompanyConfigurations/robots/MyRobot/config/controller.ini");
    const auto yarpFile = writeFile(root / "Other/robots/YarpRobot/config/controller.ini");
    ResolveRoboticsURIOptions options;
    options.excludeActivePrefix = true;
    options.packageDirs = {root.string()};
    options.defaultPackageName = "ControlLib";
    const auto resolve
        = [&] { return resolveRoboticsURI("robot://config/controller.ini", options); };
    CHECK(resolve() == fallback.string());
    yarp.set("YarpRobot");
    CHECK(resolve() == yarpFile.string());
    active.set("MyRobot");
    CHECK(resolve() == overrideFile.string());
    auto local = writeFile(cwd / "config/controller.ini");
    CHECK(resolve() == local.string());
    std::filesystem::remove(local);
    writeDirectory(local);
    CHECK(resolve() == overrideFile.string());
    active.set("MissingRobot");
    CHECK(resolve() == fallback.string());
    active.set("");
    CHECK(resolve() == yarpFile.string());
    active.set("MyRobot");
    std::filesystem::remove(overrideFile);
    writeDirectory(overrideFile);
    writeFile(root / "Nested/package/robots/MyRobot/config/controller.ini");
    writeFile(root / "robots/MyRobot/config/controller.ini");
    CHECK(resolve() == fallback.string());

    // Newly installed robots directories require refreshing discovery.
    const auto newFile = writeFile(root / "NewPackage/robots/MyRobot/config/controller.ini");
    CHECK(resolve() == fallback.string());
    clearRobotSpecificFileCache();
    CHECK(resolve() == newFile.string());
    std::filesystem::remove(newFile);
    CHECK(resolve() == fallback.string());
    writeFile(newFile);
    CHECK(resolve() == newFile.string());
    const auto first = writeFile(root / "A/robots/MyRobot/config/controller.ini");
    clearRobotSpecificFileCache();
    CHECK(resolve() == first.string());

    // A changed root set discovers overrides without refreshing the cache.
    const auto otherRoot = base / "other_share";
    const auto otherOverride = writeFile(otherRoot / "Pkg/robots/MyRobot/config/controller.ini");
    options.packageDirs = {otherRoot.string()};
    CHECK(resolve() == otherOverride.string());
    options.packageDirs = {root.string(), otherRoot.string()};
    CHECK(resolve() == otherOverride.string()); // "other_share" sorts before "share".
    std::filesystem::remove(otherOverride);
    CHECK(resolve() == first.string());

    ScopedEnvVar ament("AMENT_PREFIX_PATH");
    ament.set(base.string());
    options.packageDirs.clear();
    CHECK(resolve() == first.string());
    options.excludeEnvVars.insert("AMENT_PREFIX_PATH");
    CHECK_FALSE(resolve().has_value());
    options.packageDirs = {root.string()};
    CHECK(resolve() == first.string());

    // Explicit package URIs bypass both local files and robot overrides.
    std::string uriError;
    CHECK(resolveRoboticsURI("package://ControlLib/config/controller.ini", options, uriError)
          == fallback.string());
    active.set("../invalid");
    CHECK(resolveRoboticsURI("package://ControlLib/config/controller.ini", options, uriError)
          == fallback.string());
    active.set("MyRobot");
    CHECK_FALSE(
        resolveRoboticsURI("package://ControlLib/missing.ini", options, uriError).has_value());
    CHECK_FALSE(uriError.empty());
    options.defaultPackageName.clear();
    CHECK(resolveRoboticsURI("robot://config/controller.ini", options) == first.string());
    options.defaultPackageName = "ControlLib";

    std::string error = "old error";
    CHECK(resolveRoboticsURI("robot://config/controller.ini", options, error) == first.string());
    CHECK(error.empty());
    for (const auto& path : {"", "../outside", "/absolute", "file:/absolute"})
    {
        CHECK_FALSE(resolveRoboticsURI(std::string("robot://") + path, options, error).has_value());
        CHECK_FALSE(error.empty());
    }
    options.defaultPackageName = "a/b";
    CHECK_FALSE(resolveRoboticsURI("robot://file", options).has_value());
    options.defaultPackageName = "ControlLib";
    active.set("../outside");
    CHECK_FALSE(resolve().has_value());
    active.unset();
    yarp.unset();
    std::filesystem::remove(fallback);
    writeDirectory(fallback);
    CHECK_FALSE(resolve().has_value());
    // No package means no default lookup, even directly under a search root.
    writeFile(root / "config/controller.ini");
    options.defaultPackageName.clear();
    CHECK_FALSE(resolveRoboticsURI("robot://config/controller.ini", options).has_value());
    // Exercise the default-options overloads as well.
    auto uniqueLocal = writeFile(cwd / "local.ini");
    CHECK(resolveRoboticsURI("robot://local.ini") == uniqueLocal.string());
    CHECK(resolveRoboticsURI("robot:local.ini") == uniqueLocal.string());
    CHECK(resolveRoboticsURI("ROBOT://local.ini") == uniqueLocal.string());
    CHECK(resolveRoboticsURI("robot://local.ini", error) == uniqueLocal.string());
}

TEST_CASE("URIOverrides")
{
    using namespace ResolveRoboticsURICpp;
    ResolveRoboticsURIOptions options;
    options.packageDirs = {"custom-root"};
    options.excludeActivePrefix = true;
    options.excludeEnvVars = {"AMENT_PREFIX_PATH"};
    options.defaultPackageName = "ControlLib";
    std::string received;
    const URIResolver callback = [&](const std::string& uri,
                                     const ResolveRoboticsURIOptions& forwarded,
                                     std::string& error) -> std::optional<std::string> {
        received = uri;
        CHECK(forwarded.packageDirs == options.packageDirs);
        CHECK(forwarded.excludeActivePrefix);
        CHECK(forwarded.excludeEnvVars == options.excludeEnvVars);
        CHECK(forwarded.defaultPackageName == "ControlLib");
        CHECK(error.empty());
        return "custom-result";
    };
    for (const auto& scheme : {"custom", "robot", "package", "model", "file"})
    {
        options.uriOverrides[scheme] = callback;
        std::string error = "stale error";
        const std::string uri = std::string(scheme) + "://file.ini";
        CHECK(resolveRoboticsURI(uri, options, error) == "custom-result");
        CHECK(received == uri);
    }
    options.uriOverrides["custom"] = [](const std::string&,
                                        const ResolveRoboticsURIOptions&,
                                        std::string& error) -> std::optional<std::string> {
        error = "custom failure";
        return {};
    };
    std::string error;
    CHECK_FALSE(resolveRoboticsURI("custom://nested/file.ini", options, error));
    CHECK(error == "custom failure");
    CHECK_FALSE(resolveRoboticsURI("custom:resource", options, error));
    CHECK(error == "custom failure");
    options.uriOverrides["custom"] = callback;
    CHECK(resolveRoboticsURI("custom://nested/file.ini", options) == "custom-result");
    CHECK(resolveRoboticsURI("CUSTOM:resource", options) == "custom-result");
    CHECK_FALSE(resolveRoboticsURI("customized://file.ini", options));
    options.uriOverrides.clear();
    CHECK_FALSE(resolveRoboticsURI("robot://file.ini", options));
    for (const auto& scheme : {"", "custom://", "custom://nested/", "custom:", "CUSTOM", "1custom"})
    {
        options.uriOverrides = {{scheme, callback}};
        CHECK_FALSE(resolveRoboticsURI("custom://file.ini", options, error));
        CHECK_FALSE(error.empty());
    }
    options.uriOverrides = {{"custom", {}}};
    CHECK_FALSE(resolveRoboticsURI("custom://file.ini", options, error));
    CHECK_FALSE(error.empty());

    ResolveRoboticsURIOptions delegate;
    delegate.uriOverrides["alias"]
        = [](const std::string&, const ResolveRoboticsURIOptions& opts, std::string& err) {
              auto nested = opts;
              nested.uriOverrides.clear();
              return resolveRoboticsURI("package://missing/file.ini", nested, err);
          };
    CHECK_FALSE(resolveRoboticsURI("alias://file.ini", delegate, error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("UnsupportedSchemesAndWindowsPaths")
{
    using namespace ResolveRoboticsURICpp;
    ResolveRoboticsURIOptions options;
    options.uriOverrides.clear();
    for (const auto& uri : {"https:resource", "https://example.org/file", "robot:local.ini"})
    {
        std::string error;
        CHECK_FALSE(resolveRoboticsURI(uri, options, error));
        CHECK(error.find("Unsupported URI scheme") != std::string::npos);
    }
#ifdef _WIN32
    const auto file = writeFile(std::filesystem::temp_directory_path() / "rru_drive_path.ini");
    const std::string driveScheme(1, static_cast<char>(std::tolower(file.string()[0])));
    options.uriOverrides[driveScheme] = [](const std::string&,
                                           const ResolveRoboticsURIOptions&,
                                           std::string&) -> std::optional<std::string> {
        FAIL("A drive letter must not select a URI override");
        return {};
    };
    CHECK(resolveRoboticsURI(file.string(), options) == file.string());
    std::filesystem::remove(file);
#endif
}

TEST_CASE("ScopedURISearchOrder")
{
    using namespace ResolveRoboticsURICpp;
    std::vector<std::unique_ptr<ScopedEnvVar>> env;
    for (const auto& name : supportedEnvVars())
    {
        env.emplace_back(new ScopedEnvVar(name));
        env.back()->unset();
    }
    const auto base = std::filesystem::temp_directory_path() / "rru_cpp_search_order_test";
    struct Cleanup
    {
        std::filesystem::path base;
        ~Cleanup()
        {
            std::filesystem::remove_all(base);
        }
    } cleanup{base};
    std::filesystem::remove_all(base);
    std::vector<std::string> roots;
    for (int i = 0; i < 11; ++i)
    {
        roots.push_back((base / std::to_string(i)).string());
    }
    roots[0] = (base / "ament/share").string();
    roots[8] = (base / "prefix/share").string();
    ScopedEnvVar ament("AMENT_PREFIX_PATH");
    ament.set((base / "ament").string());
    ScopedEnvVar gazebo("GAZEBO_MODEL_PATH");
#ifdef _WIN32
    const std::string separator = ";";
#else
    const std::string separator = ":";
#endif
    gazebo.set(roots[1] + separator + roots[2] + separator + roots[1]);
    const std::vector<std::string> names{"GZ_SIM_RESOURCE_PATH",
                                         "IGN_GAZEBO_RESOURCE_PATH",
                                         "ROS_PACKAGE_PATH",
                                         "SDF_PATH",
                                         "RRU_ADDITIONAL_PATHS"};
    std::vector<std::unique_ptr<ScopedEnvVar>> overrides;
    for (size_t i = 0; i < names.size(); ++i)
    {
        overrides.emplace_back(new ScopedEnvVar(names[i]));
        overrides.back()->set(roots[i + 3]);
    }
    ScopedEnvVar prefix("CONDA_PREFIX");
    prefix.set((base / "prefix").string());
    ResolveRoboticsURIOptions options;
    options.packageDirs = {roots[9], roots[10], roots[9], roots[1]};
    auto expectedRoots = roots;
#ifdef _WIN32
    expectedRoots.insert(expectedRoots.begin() + 9, (base / "prefix/Library/share").string());
#endif
    const auto collected = getSearchPaths(options);
    CHECK(std::vector<std::string>(collected.begin(), collected.end()) == expectedRoots);
    for (const auto& scheme : {"package://", "model://"})
    {
        std::vector<std::filesystem::path> files;
        for (const auto& root : roots)
        {
            files.push_back(writeFile(std::filesystem::path(root) / "mypkg/model.urdf"));
        }
        const std::string uri = std::string(scheme) + "mypkg/model.urdf";
        for (const auto& file : files)
        {
            REQUIRE(resolveRoboticsURI(uri, options) == file.string());
            std::filesystem::remove(file);
        }
        CHECK_FALSE(resolveRoboticsURI(uri, options));
        // Reversing explicit roots must reverse the winner too.
        writeFile(files[9]);
        writeFile(files[10]);
        options.packageDirs = {roots[10], roots[9], roots[10]};
        CHECK(resolveRoboticsURI(uri, options) == files[10].string());
        std::filesystem::remove(files[9]);
        std::filesystem::remove(files[10]);
        options.packageDirs = {roots[9], roots[10], roots[9], roots[1]};
    }
}
