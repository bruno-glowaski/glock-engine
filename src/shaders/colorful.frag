#include "colorful.h.hlsl"

struct FSOutput {
  float4 color : SV_Target0;
};

FSOutput main(VSOutput input) {
  FSOutput output;
  float ellapsed_time = per_frame_uniforms.ellapsed_time;
  output.color = float4(cos(ellapsed_time), cos(ellapsed_time + PI / 2),
                        cos(ellapsed_time + PI / 3), 1.0);
  return output;
}
