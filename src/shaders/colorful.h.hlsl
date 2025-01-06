static const float PI = 3.14159265f;

struct PerFrameUniforms {
  float time;
};

[[vk::push_constant]]
cbuffer push_constants { PerFrameUniforms per_frame_uniforms; };

struct VSOutput {
  float4 position : SV_Position;
};
