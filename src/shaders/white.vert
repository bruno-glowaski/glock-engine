struct vs_in {
  float3 position : POSITION0;
};

struct vs_out {
  float4 position : SV_Position;
};

vs_out main(const vs_in input) {
  vs_out output;
  output.position = float4(input.position, 1.0);
  return output;
}
