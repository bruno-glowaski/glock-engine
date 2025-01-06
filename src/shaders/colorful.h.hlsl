static const float PI = 3.14159265f;

struct PerMeshUniforms {
  float4x4 mvp;
};

struct PerFrameUniforms {
  float time;
};

[[vk::push_constant]]
cbuffer push_constants {
  PerMeshUniforms per_mesh_uniforms;
  PerFrameUniforms per_frame_uniforms;
};

struct VSOutput {
  float4 position : SV_Position;
};
