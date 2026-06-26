cmake_minimum_required(VERSION 3.16)

project(example_cmake_package LANGUAGES CXX)

install(FILES package.xml DESTINATION share/${PROJECT_NAME})
install(FILES share/${PROJECT_NAME}/cube.urdf DESTINATION share/${PROJECT_NAME})

# ROS 2 package discovery marker.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}" "")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}"
        DESTINATION share/ament_index/resource_index/packages)
