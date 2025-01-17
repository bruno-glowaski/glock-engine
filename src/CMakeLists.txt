add_executable(engine 
  "${CMAKE_CURRENT_SOURCE_DIR}/buffer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/graphics_device.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/render_system.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/swapchain.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/window.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/materials/utils.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/materials/colorful.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/materials/simple.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

target_compile_options(engine PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
       -Wall -Werror -Wextra -Wconversion -Wsign-conversion -pedantic-errors>
  $<$<CXX_COMPILER_ID:MSVC>:
       /WX /W4 /wd4068 /wd4244>
)

target_link_libraries(engine PRIVATE
  vulkan 
  glfw 
  GPUOpen::VulkanMemoryAllocator 
  VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
)

target_include_directories(engine 
  SYSTEM PRIVATE 
  "${CMAKE_SOURCE_DIR}/libs/VulkanMemoryAllocator-Hpp/include" 
  "${CMAKE_SOURCE_DIR}/libs/stb"
)

# shader subdirectory is handled by parent CMakeLists
