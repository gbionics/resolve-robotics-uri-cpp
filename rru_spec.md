# `ResolveRoboticsURICpp` File Search Rules

`ResolveRoboticsURICpp` resolves URIs by following the rules below.

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

For scoped URIs, the resolver strips the scheme and searches for the remaining relative path under an ordered list of candidate directories.

The search order is:

1. Search paths derived from supported environment variables.
2. Search paths derived from the active environment prefixes (`VIRTUAL_ENV`, then `CONDA_PREFIX`), unless excluded.
3. Any directories passed explicitly through `ResolveRoboticsURIOptions::packageDirs`.

If multiple matching files are found, the first one in the collected list is returned.

## Environment Variables Used For Search

The resolver reads these environment variables by default:

- `AMENT_PREFIX_PATH`
- `GAZEBO_MODEL_PATH`
- `GZ_SIM_RESOURCE_PATH`
- `IGN_GAZEBO_RESOURCE_PATH`
- `ROS_PACKAGE_PATH`
- `SDF_PATH`
- `RRU_ADDITIONAL_PATHS`

Variables are searched in the order listed above. Paths within each variable retain
their listed order. Duplicate search paths keep their first occurrence.

Each variable may contain multiple paths separated by the OS path separator (`:` on POSIX, `;` on Windows).

Special handling applies to `AMENT_PREFIX_PATH`: each entry is interpreted as a prefix and `share` is appended before searching.

All other variables are searched as-is.

## Active Prefix Search Paths

By default, the resolver considers both nonempty active environment prefixes in this order:

- `VIRTUAL_ENV`
- `CONDA_PREFIX`

When both are defined, resources in `VIRTUAL_ENV` take precedence over resources in
`CONDA_PREFIX`, supporting virtual environments nested inside conda environments.

For each prefix, it searches:

- `<prefix>/share`
- On Windows only: `<prefix>/Library/share`

This makes it possible to resolve ROS-style resources installed by pure CMake/C++ projects or other tools that place files under `share/<package_name>/` in active environments.

You can disable active-prefix-based search by setting `ResolveRoboticsURIOptions::excludeActivePrefix = true`.

## Excluding Environment Variables

You can exclude specific environment variables from the default path extraction by adding their names to `ResolveRoboticsURIOptions::excludeEnvVars`.

## Explicit Additional Directories

`ResolveRoboticsURIOptions::packageDirs` adds extra directories in their supplied order to the search list.

These directories are searched in addition to default environment-based and active-prefix-based paths.

## Failure Behavior

If no matching path is found, the resolver returns an empty `std::optional` and fills the error message when provided.

# URI overrides

Overrides are checked before normal URI/path processing. Keys are lowercase URI
scheme names without ":" or "//", e.g. `"home"`. The parsed scheme is matched
exactly, so `home://file` and `home:file` select the same handler; `homework://file`
does not. Override selection is case-insensitive. Windows drive paths bypass overrides.
The callback receives the full input URI and search options. Its result or failure
is final; no other handler or normal lookup is tried afterward. Empty or invalid
scheme names are rejected.

`ResolveRoboticsURIOptions::uriOverrides` is a map from scheme names to
`URIResolver` callbacks, initialized with a `robot://` callback. Their signature is
`std::optional<std::string>(const std::string& uri, const ResolveRoboticsURIOptions&,
std::string& errorMessage)`. Empty callbacks are invalid. The error is cleared
before dispatch; callbacks set it on failure. Clear the map to disable overrides,
erase one scheme to remove a handler, or assign a callback to add/replace a handler.

Callbacks can override built-in schemes as well as custom schemes. When delegating to
the resolver, remove the matching scheme override (or disable overrides) to avoid recursion.

# Robot-specific files via `robot://`

`robot://<relative-path>` resolves a robot-specific file. The optional default
package is supplied separately (`ResolveRoboticsURIOptions::defaultPackageName`).
The URI authority and path together form the relative path, so
`robot://config/controller.ini` means `config/controller.ini`.

1. Return the regular file relative to the working directory, if present.
2. Select the robot from nonempty `ACTIVE_ROBOT_NAME`, falling back to nonempty
   `YARP_ROBOT_NAME`. Search `<root>/<package>/robots/<robot>/<relative-path>`
   under every immediate package directory in the normal search roots.
3. If a default package was supplied, search `<root>/<default-package>/<relative-path>`.
   Otherwise skip this step.

Only regular files match. Results are absolute canonical paths. Empty, absolute,
parent-traversing, or URI-valued relative paths are invalid. Nonempty robot and
package names must each be a single directory component. Multiple matches in the
winning tier return the first distinct canonical path in lexical order.
Missing files and invalid inputs return an empty optional and set the error message.

Discovery of package `robots` directories is cached by canonical search-root set
(up to 128 entries). File results and robot selection are not cached. After
installing new override packages, call `clearRobotSpecificFileCache()`
to refresh discovery.

# Command-line behaviour

The CLI passes its input to the URI resolver. Use `robot://config/controller.ini`
for robot lookup and `--default-package-name NAME` to add a default package. Plain paths
and other URI schemes retain normal resolution. Search path and exclusion flags
apply to robot lookup too.
