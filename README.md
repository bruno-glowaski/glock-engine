# Glock Engine

A simple Vulkan engine developed on C++23 and GLFW3. Focused on single threaded execution. Made for fun.

## Compiling

```sh
cmake -B build && cmake --build build
```

To run, execute:

```sh
cd build && bin/engine
```

## Todo

- [ ] Remove `vk` preffixes from Vulkan primitive members;
- [ ] Add texture loading;
- [ ] Render a skybox;
- [ ] Refactor models into their own class;
- [ ] Add music;
- [ ] Refactor resource loading;
- [ ] Render sprite text;
- [ ] Refactor framework into its own class.

