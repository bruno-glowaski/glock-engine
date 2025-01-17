#include "vaporwave_skybox.h.hlsl"

struct VSInput {
  float3 position : POSITION0;
};

VSOutput main(const VSInput input) {
  VSOutput output;
  output.position_model = float4(input.position, 1.0);
  output.position_clip = mul(per_mesh_uniforms.mvp, float4(input.position, 1.0));
  return output;
}
