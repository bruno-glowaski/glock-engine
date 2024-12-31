add_executable(engine 
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/window.cpp"
)

target_compile_options(engine PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Werror -Wextra -Wconversion -Wsign-conversion -pedantic-errors>
     $<$<CXX_COMPILER_ID:MSVC>:
          /WX /W4 /wd4068 /wd4244>)

target_link_libraries(engine PRIVATE 
  vulkan 
  glfw 
  GPUOpen::VulkanMemoryAllocator 
  VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp)

# shader subdirectory is handled by parent CMakeLists
