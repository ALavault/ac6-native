// The decoder's per-descriptor result, as the contract's microexec-style
// artefact. Not a micro-execution: this runs the PORT over real retail data and
// records what it produced, which is what the entry cites.
#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_geometry.h"
#include <cstdio>
#include <fstream>
#include <vector>
static std::vector<std::uint8_t> Read(const char* p){std::ifstream i(p,std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(i)),std::istreambuf_iterator<char>());}
int main(int argc,char**argv){
  auto blob=Read(argv[1]);
  auto dir=ac6::retail::ModelDirectory::open(blob.data(),blob.size());
  if(!dir) return 1;
  std::printf("entry\tsub\trecord\tdesc\tstride\tverts\tindices\trestarts\tminx\tmaxx\tminy\tmaxy\tminz\tmaxz\n");
  std::size_t total=0, decoded=0, restarts=0;
  for(std::uint32_t id=0;id<dir->count();++id){
    auto e=dir->entry(id); if(!e) continue;
    const std::uint8_t* fhm=blob.data()+e->offset;
    ac6::retail::ContainerIndex ix{};
    if(!ac6::retail::parse_container_index(ix,fhm,e->size,(std::uint32_t)e->offset)) continue;
    for(std::uint32_t j=0;j<ix.count;++j){
      std::uint32_t at=ac6::retail::container_entry(ix,fhm,e->size,j); if(!at) continue;
      std::size_t off=at-(std::uint32_t)e->offset; if(off+8>e->size) continue;
      const std::uint8_t* sub=fhm+off;
      if(sub[0]!='N'||sub[1]!='D'||sub[2]!='X'||sub[3]!='R') continue;
      std::uint32_t len=ac6::retail::container_entry_length(ix,fhm,e->size,j);
      auto c=ac6::retail::NdxrContainer::Open(sub,len); if(!c) continue;
      for(std::uint16_t r=0;r<c->record_count();++r){
        auto rec=c->Record(r); if(!rec) continue;
        for(std::uint16_t k=0;k<rec->descriptor_count;++k){
          auto d=c->Descriptor(*rec,k); if(!d) continue;
          ++total;
          auto m=ac6::retail::decode_ndxr_descriptor(*c,sub,len,*d);
          if(!m) continue;
          ++decoded;
          std::size_t rs=0; for(auto v:m->indices) if(v==ac6::retail::kStripRestart) ++rs;
          restarts+=rs;
          std::printf("%u\t%u\t%u\t%u\t%u\t%zu\t%zu\t%zu\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\n",
            id,j,r,k,d->vertex_stride,m->positions.size(),m->indices.size(),rs,
            m->bounds.min_x,m->bounds.max_x,m->bounds.min_y,m->bounds.max_y,
            m->bounds.min_z,m->bounds.max_z);
        }
      }
    }
  }
  std::fprintf(stderr,"descriptors %zu decoded %zu restarts %zu\n",total,decoded,restarts);
  return decoded==total?0:1;}
