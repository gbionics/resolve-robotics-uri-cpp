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
    for (const auto& prefix : {"custom://", "robot://", "package://", "model://", "file://"})
    {
        options.uri_override[prefix] = callback;
        std::string error = "stale error";
        const std::string uri = std::string(prefix) + "file.ini";
        CHECK(resolveRoboticsURI(uri, options, error) == "custom-result");
        CHECK(received == uri);
    }
    options.uri_override["custom://nested/"]
        = [](const std::string&,
             const ResolveRoboticsURIOptions&,
             std::string& error) -> std::optional<std::string> {
        error = "custom failure";
        return {};
    };
    std::string error;
    CHECK_FALSE(resolveRoboticsURI("custom://nested/file.ini", options, error));
    CHECK(error == "custom failure");
    CHECK(resolveRoboticsURI("custom://file.ini", options) == "custom-result");
    options.uri_override.clear();
    CHECK_FALSE(resolveRoboticsURI("robot://file.ini", options));
    options.uri_override[""] = callback;
    CHECK_FALSE(resolveRoboticsURI("custom://file.ini", options, error));
    CHECK_FALSE(error.empty());
    options.uri_override = {{"custom://", {}}};
    CHECK_FALSE(resolveRoboticsURI("custom://file.ini", options, error));
    CHECK_FALSE(error.empty());

    ResolveRoboticsURIOptions delegate;
    delegate.uri_override["alias://"]
        = [](const std::string&, const ResolveRoboticsURIOptions& opts, std::string& err) {
              auto nested = opts;
              nested.uri_override.clear();
              return resolveRoboticsURI("package://missing/file.ini", nested, err);
          };
    CHECK_FALSE(resolveRoboticsURI("alias://file.ini", delegate, error));
    CHECK_FALSE(error.empty());
}
