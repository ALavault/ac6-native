#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/retail_scenario.h"
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
static std::vector<std::uint8_t> Read(const char* p){std::ifstream i(p,std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(i)),std::istreambuf_iterator<char>());}
int main(int argc,char**argv){
  auto blob=Read(argv[1]); auto pay=Read(argv[2]);
  auto dir=ac6::retail::ModelDirectory::open(blob.data(),blob.size());
  auto ps=ac6::retail::ScenarioPayload::open(pay);
  auto sc=ac6::retail::MissionScenario::parse(*ps);
  std::map<std::uint8_t,int> uses;
  for(const auto&u:sc->units()) for(const auto&b:u.model_bindings) if(b.has_model()) uses[b.primary]++;
  for(const auto&[id,count]:uses){
    auto e=dir->entry(id); if(!e) continue;
    const std::uint8_t* fhm=blob.data()+e->offset;
    ac6::retail::ContainerIndex ix{};
    if(!ac6::retail::parse_container_index(ix,fhm,e->size,(std::uint32_t)e->offset)) continue;
    std::set<std::string> names;
    for(std::uint32_t j=0;j<ix.count;++j){
      std::uint32_t at=ac6::retail::container_entry(ix,fhm,e->size,j); if(!at) continue;
      std::size_t off=at-(std::uint32_t)e->offset; if(off+8>e->size) continue;
      const std::uint8_t* sub=fhm+off;
      if(sub[0]!='N'||sub[1]!='D'||sub[2]!='X'||sub[3]!='R') continue;
      std::uint32_t len=ac6::retail::container_entry_length(ix,fhm,e->size,j);
      auto c=ac6::retail::NdxrContainer::Open(sub,len); if(!c) continue;
      for(std::uint16_t r=0;r<c->record_count();++r){
        auto rec=c->Record(r); if(!rec) continue;
        if(!rec->name.empty()) names.insert(std::string(rec->name));
      }
    }
    std::printf("id %3u  used %3d  :",id,count);
    int n=0; for(const auto&s:names){ if(n++>=6){std::printf(" ...(%zu)",names.size());break;} std::printf(" %s",s.c_str()); }
    std::printf("\n");
  }
  return 0;}
