struct vs_in {
  float4 position : POSITION0;
};

struct vs_out {
  float4 position : SV_Position;
};

vs_out main(const vs_in input) {
  vs_out output;
  output.position = input.position;
  return output;
}
