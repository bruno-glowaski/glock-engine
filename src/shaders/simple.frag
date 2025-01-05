#include "simple.h.hlsl"

struct FSOutput {
  float4 color : SV_Target0;
};

FSOutput main(VSOutput input) {
  FSOutput output;
  output.color = float4(per_mesh_uniforms.color, 1.0);
  return output;
}
