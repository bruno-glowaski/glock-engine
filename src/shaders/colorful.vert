#include "colorful.h.hlsl"

struct VSInput {
  float3 position : POSITION0;
};

VSOutput main(const VSInput input) {
  VSOutput output;
  output.position = mul(per_mesh_uniforms.mvp, float4(input.position, 1.0));
  return output;
}
