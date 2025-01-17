#include "vaporwave_skybox.h.hlsl"

struct FSOutput {
  float4 color : SV_Target0;
};

float2 dir_to_sphere(float3 norm_dir) {
  float theta = asin(norm_dir.y);
  float phi =
      sign(norm_dir.z) * acos(sqrt(pow(norm_dir.x, 2) + pow(norm_dir.z, 2)));
  return float2(phi, theta);
}

float3 to_linear(float3 sRGB) {
  bool3 cutoff = sRGB < float3(0.04045);
  float3 higher = pow((sRGB + float3(0.055)) / float3(1.055), float3(2.4));
  float3 lower = sRGB / float3(12.92);

  return lerp(higher, lower, float3(cutoff));
}

FSOutput main(VSOutput input) {
  FSOutput output;

  const float3 sky_top_color = to_linear(float3(0.13, 0.02, 0.16));
  const float3 sky_horizon_color = to_linear(float3(0.48, 0.15, 0.36));
  const float sky_curve = 0.1 * 2 / PI;
  const float3 ground_horizon_color = to_linear(float3(0.20, 0.07, 0.44));
  const float3 ground_bottom_color = to_linear(float3(0.05, 0.04, 0.18));
  const float ground_curve = 0.5 * 2 / PI;
  const float3 sun_color = to_linear(float3(0.99, 0.76, 0.5));
  const float3 sun_size = 0.5 / PI;
  const float3 sun_smooth = 0.01 / PI;

  // TODO: use position in relation to world instead of model
  float2 lat_lon = dir_to_sphere(normalize(input.position_model.xyz));

  float3 sky_color = lerp(sky_horizon_color, sky_top_color,
                          clamp(lat_lon.y / sky_curve, 0.0, 1.0));
  float3 ground_color = lerp(ground_horizon_color, ground_bottom_color,
                             clamp(-lat_lon.y / ground_curve, 0.0, 1.0));
  float sun_distance = distance(normalize(input.position_model.xyz),
                                normalize(float3(0.0, 0.1, 1.0)));
  float sun_mask = smoothstep(sun_distance - sun_smooth,
                              sun_distance + sun_smooth, sun_size);
  float3 final_color = lerp(ground_color, lerp(sky_color, sun_color, sun_mask),
                            step(0.0, lat_lon.y));

  output.color = float4(final_color, 1.0);
  return output;
}
