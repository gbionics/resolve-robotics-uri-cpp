# File Search Rules

`resolve-robotics-uri-cpp` resolves URIs by following the rules below.

## Supported URI Schemes

The resolver accepts these schemes:

- `file://`
- `package://`
- `model://`

If a path is passed without a scheme, it is treated as a local file path and checked directly.

## `file://` Resolution

For `file://` URIs, the path is converted to a local filesystem path and checked directly.
The path can point to either a file or a directory, and it must exist.

## `package://` and `model://` Resolution

For scoped URIs, the resolver strips the scheme and searches for the remaining relative path under a set of candidate directories.

The search order is:

1. Search paths derived from supported environment variables.
2. Search paths derived from the active C++ environment prefix (`CONDA_PREFIX`, or fallback `VIRTUAL_ENV`), unless excluded.
3. Any directories passed explicitly through `ResolveRoboticsURIOptions::packageDirs`.

If multiple matching files are found, the first one encountered in the internal path set iteration is returned.

## Environment Variables Used For Search

The resolver reads these environment variables by default:

- `AMENT_PREFIX_PATH`
- `GAZEBO_MODEL_PATH`
- `GZ_SIM_RESOURCE_PATH`
- `IGN_GAZEBO_RESOURCE_PATH`
- `ROS_PACKAGE_PATH`
- `SDF_PATH`
- `RRU_ADDITIONAL_PATHS`

Each variable may contain multiple paths separated by the OS path separator (`:` on POSIX, `;` on Windows).

Special handling applies to `AMENT_PREFIX_PATH`: each entry is interpreted as a prefix and `share` is appended before searching.

All other variables are searched as-is.

## Active Prefix Search Paths

By default, the resolver considers the active environment prefix:

- `CONDA_PREFIX`
- if `CONDA_PREFIX` is not defined: `VIRTUAL_ENV`

For that prefix, it searches:

- `<prefix>/share`
- On Windows only: `<prefix>/Library/share`

This makes it possible to resolve ROS-style resources installed by pure CMake/C++ projects or other tools that place files under `share/<package_name>/` in active environments.

You can disable active-prefix-based search by setting `ResolveRoboticsURIOptions::excludeActivePrefix = true`.

## Excluding Environment Variables

You can exclude specific environment variables from the default path extraction by adding their names to `ResolveRoboticsURIOptions::excludeEnvVars`.

## Explicit Additional Directories

`ResolveRoboticsURIOptions::packageDirs` adds extra directories to the search set.

These directories are searched in addition to default environment-based and active-prefix-based paths.

## Failure Behavior

If no matching path is found, the resolver returns an empty `std::optional` and fills the error message when provided.
