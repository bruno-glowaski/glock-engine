#include "utils.hpp"

#include <fstream>
#include <ios>

vk::UniqueShaderModule loadShaderFromPath(const std::string_view path,
                                          vk::Device device) {
  std::ifstream file(path.data(), std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("failed to open file {}", path));
  }
  std::vector<char> shaderData{std::istreambuf_iterator(file), {}};
  return device.createShaderModuleUnique(
      vk::ShaderModuleCreateInfo{}
          .setCodeSize(shaderData.size())
          .setPCode(reinterpret_cast<const uint32_t *>(shaderData.data())));
}
