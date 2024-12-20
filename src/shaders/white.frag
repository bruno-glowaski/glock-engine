struct ps_in {
  float4 position : SV_Position;
};

struct ps_out {
  float4 color : SV_Target0;
};

ps_out main(ps_in input) {
  input.position.x++;
  ps_out output;
  output.color = float4(1.0,1.0,1.0,1.0);
  return output;
}
