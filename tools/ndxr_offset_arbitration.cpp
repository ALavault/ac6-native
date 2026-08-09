#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_container.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
int main(int argc, char** argv) {
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<std::uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  auto dir = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  int n=0;
  int v_file=0, v_first=0, v_second=0, v_third=0;
  int i_file=0, i_first=0, i_second=0, i_third=0;
  for (std::uint32_t id=0; id<dir->count(); ++id) {
    auto e = dir->entry(id); if (!e) continue;
    const std::uint8_t* fhm = blob.data()+e->offset;
    ac6::retail::ContainerIndex ix{};
    if (!ac6::retail::parse_container_index(ix, fhm, e->size, (std::uint32_t)e->offset)) continue;
    for (std::uint32_t j=0;j<ix.count;++j) {
      std::uint32_t at = ac6::retail::container_entry(ix, fhm, e->size, j);
      if (!at) continue;
      std::size_t off = at - (std::uint32_t)e->offset;
      if (off+8 > e->size) continue;
      const std::uint8_t* sub = fhm+off;
      if (sub[0]!='N'||sub[1]!='D'||sub[2]!='X'||sub[3]!='R') continue;
      std::uint32_t len = ac6::retail::container_entry_length(ix, fhm, e->size, j);
      auto c = ac6::retail::NdxrContainer::Open(sub, len);
      if (!c) continue;
      const auto& s = c->sections();
      for (std::uint16_t r=0;r<c->record_count();++r) {
        auto rec = c->Record(r); if (!rec) continue;
        for (std::uint16_t k=0;k<rec->descriptor_count;++k) {
          auto d = c->Descriptor(*rec,k); if (!d) continue;
          ++n;
          const std::size_t vbytes = std::size_t(d->vertex_count) * d->vertex_stride;
          const std::size_t ibytes = std::size_t(d->index_count) * 2u;
          const std::size_t vo = d->vertex_offset, io = std::size_t(d->index_offset);
          // A CONTAINMENT TEST DOES NOT DISCRIMINATE: every base passed 1227/1227
          // because the spans are small against the container. Decode the first
          // three floats of every vertex instead and ask whether they look like
          // positions -- finite, and inside a box no aircraft-sized model leaves.
          auto plausible=[&](std::size_t base){
            if (base + vo + vbytes > len) return false;
            const std::uint8_t* v = sub + base + vo;
            for (std::uint32_t q=0;q<d->vertex_count;++q) {
              const std::uint8_t* p2 = v + std::size_t(q)*d->vertex_stride;
              for (int c2=0;c2<3;++c2) {
                std::uint32_t w = (std::uint32_t(p2[c2*4])<<24)|(std::uint32_t(p2[c2*4+1])<<16)
                                | (std::uint32_t(p2[c2*4+2])<<8)|p2[c2*4+3];
                float fv; __builtin_memcpy(&fv,&w,4);
                if (!(fv==fv) || fv>1e6f || fv<-1e6f) return false;
              }
            }
            return true; };
          auto fits=[&](std::size_t base, std::size_t off2, std::size_t bytes){
            (void)off2;(void)bytes; return plausible(base); };
          if (fits(0, vo, vbytes)) ++v_file;
          if (fits(s.first, vo, vbytes)) ++v_first;
          if (fits(s.second, vo, vbytes)) ++v_second;
          if (s.third && fits(s.third, vo, vbytes)) ++v_third;
          // Indices: 16-bit, and StartIndex is index_offset >> 1 per 0x823648C4.
          // The discriminator is that every index must address a vertex of THIS
          // descriptor -- below vertex_count -- which a wrong base fails fast.
          auto idx_ok=[&](std::size_t base, bool shift){
            const std::size_t start = shift ? (io >> 1) : io;
            const std::size_t at = base + start*2u;
            if (at + ibytes > len) return false;
            const std::uint8_t* q = sub + at;
            for (std::uint32_t t=0;t<d->index_count;++t) {
              std::uint32_t v2 = (std::uint32_t(q[t*2])<<8)|q[t*2+1];
              if (v2 >= d->vertex_count) return false;
            }
            return true; };
          // TWO READINGS of an index, and descriptor 0 cannot tell them apart
          // because its vertex_offset is 0. Descriptor 1 can: v_off=37604 is
          // exactly descriptor 0's vertex bytes, so the arrays accumulate.
          //   RELATIVE: the index counts from this descriptor's own first vertex
          //             (D3D's BaseVertexIndex = vertex_offset / stride)
          //   ABSOLUTE: the index counts from the start of the vertex section
          const std::size_t base_vertex = vo / d->vertex_stride;
          const std::size_t section_vertices = (s.end - s.second) / d->vertex_stride;
          auto idx_rel=[&]{
            const std::size_t at = s.first + io;
            if (at + ibytes > len) return false;
            const std::uint8_t* q = sub + at;
            for (std::uint32_t t=0;t<d->index_count;++t) {
              std::uint32_t v2=(std::uint32_t(q[t*2])<<8)|q[t*2+1];
              if (v2 == 0xFFFFu) continue;   // STRIP RESTART, not an index
              if (v2 >= d->vertex_count) return false; }
            return true; };
          auto idx_abs=[&]{
            const std::size_t at = s.first + io;
            if (at + ibytes > len) return false;
            const std::uint8_t* q = sub + at;
            for (std::uint32_t t=0;t<d->index_count;++t) {
              std::uint32_t v2=(std::uint32_t(q[t*2])<<8)|q[t*2+1];
              if (v2 == 0xFFFFu) continue;
              if (v2 < base_vertex || v2 >= base_vertex + d->vertex_count) return false; }
            return true; };
          auto idx_section=[&]{
            const std::size_t at = s.first + io;
            if (at + ibytes > len) return false;
            const std::uint8_t* q = sub + at;
            for (std::uint32_t t=0;t<d->index_count;++t) {
              std::uint32_t v2=(std::uint32_t(q[t*2])<<8)|q[t*2+1];
              if (v2 == 0xFFFFu) continue;
              if (v2 >= section_vertices) return false; }
            return true; };
          if (idx_rel()) ++i_file;
          if (idx_abs()) ++i_first;
          if (idx_section()) ++i_second;
        }
      }
    }
  }
  std::printf("descriptors %d\n", n);
  std::printf("  vertex positions PLAUSIBLE, base = file:%d  first:%d  second:%d  third:%d\n",
              v_file, v_first, v_second, v_third);
  std::printf("  indices at sections.first + index_offset --\n");
  std::printf("      RELATIVE to the descriptor's own vertices : %d\n", i_file);
  std::printf("      ABSOLUTE, inside [base_vertex, +count)    : %d\n", i_first);
  std::printf("      anywhere in the vertex section            : %d\n", i_second);
  return 0;
}
