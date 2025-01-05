#pragma once

#include "vulkan/vulkan.hpp"

vk::UniqueShaderModule loadShaderFromPath(const std::string_view path,
                                          vk::Device device);
