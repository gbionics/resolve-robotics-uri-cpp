# example_cmake_package

This example shows how a pure CMake/C++ project can install a ROS-style resource into `share/` so `resolve-robotics-uri-cpp` can find it with `package://example_cmake_package/cube.urdf`.

The install step places:

- `share/example_cmake_package/cube.urdf`
- `share/example_cmake_package/package.xml`
- `share/ament_index/resource_index/packages/example_cmake_package`

That layout is compatible with ROS resource discovery and with `resolve-robotics-uri-cpp` default search rules.

## Build and Install

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX
cmake --build build
cmake --install build
```

If you are not using conda, replace `$CONDA_PREFIX` with your chosen prefix, and set `AMENT_PREFIX_PATH` to your install prefix before resolving.

## Try It

```bash
resolve-robotics-uri-cpp package://example_cmake_package/cube.urdf
```
