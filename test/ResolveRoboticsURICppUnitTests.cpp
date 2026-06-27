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

std::filesystem::path writeFile(const std::filesystem::path& filePath,
                                const std::string& content = "test")
{
    std::filesystem::create_directories(filePath.parent_path());
    std::ofstream output(filePath);
    output << content;
    return filePath;
}

std::filesystem::path writeDirectory(const std::filesystem::path& directoryPath)
{
    std::filesystem::create_directories(directoryPath);
    return directoryPath;
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

    const std::filesystem::path prefixPath =
        std::filesystem::temp_directory_path() / "rru_cpp_conda_prefix_test";
    const std::filesystem::path cubeUrdf =
        writeFile(prefixPath / "share" / "example_cpp_package" / "cube.urdf", "<robot/>");

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

    const std::filesystem::path prefixPath =
        std::filesystem::temp_directory_path() / "rru_cpp_virtualenv_prefix_test";
    const std::filesystem::path cubeUrdf =
        writeFile(prefixPath / "share" / "example_cpp_package" / "cube.urdf", "<robot/>");

    virtualEnv.set(prefixPath.string());

    auto resolved =
        ResolveRoboticsURICpp::resolveRoboticsURI("package://example_cpp_package/cube.urdf");
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == cubeUrdf.string());

    std::filesystem::remove_all(prefixPath);
}

TEST_CASE("ExcludeEnvVar")
{
    ScopedEnvVar gazeboModelPath("GAZEBO_MODEL_PATH");
    gazeboModelPath.unset();

    const std::filesystem::path modelDir =
        std::filesystem::temp_directory_path() / "rru_cpp_gazebo_model_path_test";
    const std::filesystem::path modelFile = writeFile(modelDir / "example_model");

    gazeboModelPath.set(modelDir.string());

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI("model://example_model");
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == modelFile.string());

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    options.excludeEnvVars.insert("GAZEBO_MODEL_PATH");
    CHECK_FALSE(ResolveRoboticsURICpp::resolveRoboticsURI("model://example_model", options)
                    .has_value());

    std::filesystem::remove_all(modelDir);
}

TEST_CASE("AdditionalPackageDirs")
{
    const std::filesystem::path additionalDir =
        std::filesystem::temp_directory_path() / "rru_cpp_package_dirs_test";
    const std::filesystem::path cubeUrdf =
        writeFile(additionalDir / "example_cpp_package" / "cube.urdf", "<robot/>");

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    options.excludeActivePrefix = true;
    options.packageDirs.push_back(additionalDir.string());

    auto resolved =
        ResolveRoboticsURICpp::resolveRoboticsURI("package://example_cpp_package/cube.urdf", options);
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == cubeUrdf.string());

    std::filesystem::remove_all(additionalDir);
}

TEST_CASE("ResolveExistingDirectoryPath")
{
    const std::filesystem::path directoryPath =
        writeDirectory(std::filesystem::temp_directory_path() / "rru_cpp_existing_dir_test"
                       / "nested_dir");

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI(directoryPath.string());
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == directoryPath.string());

    std::filesystem::remove_all(std::filesystem::temp_directory_path() / "rru_cpp_existing_dir_test");
}

TEST_CASE("ResolveExistingDirectoryFileUri")
{
    const std::filesystem::path directoryPath =
        writeDirectory(std::filesystem::temp_directory_path() / "rru_cpp_file_uri_dir_test"
                       / "nested_dir");

    auto resolved = ResolveRoboticsURICpp::resolveRoboticsURI(fileUriFromPath(directoryPath));
    REQUIRE(resolved.has_value());
    CHECK(std::filesystem::equivalent(std::filesystem::path(resolved.value()), directoryPath));

    std::filesystem::remove_all(std::filesystem::temp_directory_path() / "rru_cpp_file_uri_dir_test");
}

TEST_CASE("ResolveDirectoryFromPackageUri")
{
    const std::filesystem::path additionalDir =
        std::filesystem::temp_directory_path() / "rru_cpp_package_dir_uri_test";
    const std::filesystem::path packageDirectory =
        writeDirectory(additionalDir / "example_cpp_package" / "meshes");

    ResolveRoboticsURICpp::ResolveRoboticsURIOptions options;
    options.excludeActivePrefix = true;
    options.packageDirs.push_back(additionalDir.string());

    auto resolved =
        ResolveRoboticsURICpp::resolveRoboticsURI("package://example_cpp_package/meshes", options);
    REQUIRE(resolved.has_value());
    CHECK(resolved.value() == packageDirectory.string());

    std::filesystem::remove_all(additionalDir);
}
