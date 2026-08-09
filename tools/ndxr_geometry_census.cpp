#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_container.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <vector>
int main(int argc, char** argv) {
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<std::uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  auto dir = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  int containers=0, records=0, descs=0, zero_stride=0, strip_ok=0;
  long long declared_records=0, declared_descs=0; int refused_records=0, refused_descs=0, relocated=0;
  long long verts=0, idx=0;
  std::map<std::uint32_t,int> strides; std::map<std::uint32_t,int> formats;
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
      ++containers;
      declared_records += c->record_count();
      for (std::uint16_t r=0;r<c->record_count();++r) {
        auto rec = c->Record(r);
        if (!rec) { ++refused_records; continue; }
        ++records;
        declared_descs += rec->descriptor_count;
        if (rec->relocated) ++relocated;
        for (std::uint16_t k=0;k<rec->descriptor_count;++k) {
          auto d = c->Descriptor(*rec,k);
          if (!d) { ++refused_descs; continue; }
          ++descs; verts += d->vertex_count; idx += d->index_count;
          strides[d->vertex_stride]++;
          formats[(std::uint32_t(d->format_hi)<<8)|d->format_lo]++;
          if (d->vertex_stride==0) ++zero_stride;
          if (d->index_count>=3) ++strip_ok;
        }
      }
    }
  }
  std::printf("containers opened %d\n", containers);
  std::printf("  records:     declared %lld  served %d  REFUSED %d\n", declared_records, records, refused_records);
  std::printf("  descriptors: declared %lld  served %d  REFUSED %d\n", declared_descs, descs, refused_descs);
  std::printf("  records flagged relocated: %d\n", relocated);
  std::printf("  total vertices %lld, total indices %lld\n", verts, idx);
  std::printf("  descriptors with stride 0 (format outside the tables): %d\n", zero_stride);
  std::printf("  descriptors with >=3 indices (a strip can be drawn): %d\n", strip_ok);
  std::printf("  distinct strides: %zu  ->", strides.size());
  for (auto& kv : strides) std::printf(" %u(x%d)", kv.first, kv.second);
  std::printf("\n  format codes:"); for (auto& kv : formats) std::printf(" [hi=%02X lo=%02X]x%d", kv.first>>8, kv.first&0xFF, kv.second); std::printf("\n");
  return 0;
}
