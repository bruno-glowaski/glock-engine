struct PerMeshUniforms {
  float3 color;
};

cbuffer per_mesh_uniforms : register(b0, space0) {
  PerMeshUniforms per_mesh_uniforms;
}

struct VSOutput {
  float4 position : SV_Position;
};
