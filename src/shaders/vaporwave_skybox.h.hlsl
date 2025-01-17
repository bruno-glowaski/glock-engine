static const float PI = 3.14159265f;

struct PerMeshUniforms {
  float4x4 mvp;
};

[[vk::push_constant]]
cbuffer push_constants { PerMeshUniforms per_mesh_uniforms; };

// Eventually use world position.
struct VSOutput {
  float4 position_clip : SV_Position;
  float4 position_model : POSITION;
};
