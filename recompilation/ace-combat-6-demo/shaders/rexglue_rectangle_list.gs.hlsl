// Test-only adaptation of ReXGlue VulkanPipelineCache::GetGeometryShader for
// PipelineGeometryShader::kRectangleList, constrained to the reached PAL demo
// interface (position only, no interpolators or clip distances).
struct Vertex {
  float4 position : SV_Position;
};

[maxvertexcount(4)]
void main(triangle Vertex input[3], inout TriangleStream<Vertex> output) {
  float2 edge12 = input[2].position.xy - input[1].position.xy;
  float2 edge20 = input[0].position.xy - input[2].position.xy;
  float2 edge01 = input[1].position.xy - input[0].position.xy;
  float3 length_squared = {
      dot(edge12, edge12), dot(edge20, edge20), dot(edge01, edge01)};
  uint first = length_squared.x > length_squared.y &&
                       length_squared.x > length_squared.z
                   ? 0U
                   : (length_squared.y > length_squared.z ? 1U : 2U);
  uint indices[3] = {first, (first + 1U) % 3U, (first + 2U) % 3U};
  Vertex vertex;
  [unroll]
  for (uint i = 0U; i < 3U; ++i) {
    vertex.position = input[indices[i]].position;
    output.Append(vertex);
  }
  vertex.position = -input[indices[0]].position +
                    input[indices[1]].position + input[indices[2]].position;
  output.Append(vertex);
  output.RestartStrip();
}
