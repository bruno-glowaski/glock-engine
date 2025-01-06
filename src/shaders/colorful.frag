#include "colorful.h.hlsl"

struct FSOutput {
  float4 color : SV_Target0;
};

float3 hsv_to_rgb(float3 src) {
  const float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  float3 p = abs(frac(src.xxx + K.xyz) * 6.0 - K.www);
  return src.z * lerp(K.xxx, saturate(p - K.xxx), src.y);
}

FSOutput main(VSOutput input) {
  const float speed = 0.05;
  FSOutput output;
  output.color = float4(
      hsv_to_rgb(float3(per_frame_uniforms.time * speed, 0.75, 0.75)), 1.0);
  return output;
}

