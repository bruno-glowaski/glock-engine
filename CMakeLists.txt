cmake_minimum_required(VERSION 3.16)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(glock-engine)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
include(AddShaderLibrary)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/VulkanMemoryAllocator-Hpp/VulkanMemoryAllocator)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/VulkanMemoryAllocator-Hpp)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/stb)

add_subdirectory(${CMAKE_SOURCE_DIR}/src bin)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/shaders assets/shaders)
