struct Vertex {
  float4 position : SV_Position;
  [[vk::location(0)]] float4 interpolator0 : TEXCOORD0;
  [[vk::location(1)]] float4 interpolator1 : TEXCOORD1;
};

[maxvertexcount(4)]
void main(lineadj Vertex input[4], inout TriangleStream<Vertex> output) {
  uint order[4] = {0U, 1U, 3U, 2U};
  [unroll]
  for (uint i = 0U; i < 4U; ++i) {
    output.Append(input[order[i]]);
  }
  output.RestartStrip();
}
