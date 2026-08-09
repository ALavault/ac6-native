#include "ac6/retail_ndxr_container.h"
#include <cstdio>
#include <cstdint>
#include <vector>
#include <fstream>
#include <map>
#include <string>
int main(int argc, char** argv) {
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  auto be = [&](std::size_t o){ return (std::uint32_t)d[o]<<24|(std::uint32_t)d[o+1]<<16|(std::uint32_t)d[o+2]<<8|d[o+3]; };
  // every NDXR occurrence, opened with the bytes from there to the end of its MDLP entry
  std::uint32_t count = be(4), table = be(0xC), data = be(0x10);
  std::vector<std::uint32_t> offs; for (std::uint32_t i=0;i<count;++i) offs.push_back(be(table+4*i));
  int tried=0, ok=0; std::map<std::string,int> why;
  for (std::size_t i=0;i<offs.size();++i) {
    std::size_t a = data+offs[i];
    std::size_t b = (i+1<offs.size()) ? data+offs[i+1] : d.size();
    for (std::size_t p=a; p+4<=b; ++p) {
      if (!(d[p]=='N'&&d[p+1]=='D'&&d[p+2]=='X'&&d[p+3]=='R')) continue;
      ++tried;
      // TRIM TO THE CONTAINER'S OWN DECLARED LENGTH at +0x04. Open requires the
      // buffer to be exactly that long -- a guard the file itself says retail
      // does not have -- so an embedded container must be trimmed by its caller.
      std::size_t declared = (std::size_t)((std::uint32_t)d[p+4]<<24|(std::uint32_t)d[p+5]<<16|(std::uint32_t)d[p+6]<<8|d[p+7]);
      std::size_t span = (declared && p+declared<=b) ? declared : (b-p);
      ac6::retail::NdxrRefusal r{};
      auto c = ac6::retail::NdxrContainer::Open(&d[p], span, &r);
      if (c) { ++ok; why["OPENED"]++; }
      else why[std::string(ac6::retail::RefusalToString(r))]++;
    }
  }
  std::printf("NDXR occurrences tried=%d opened=%d\n", tried, ok);
  for (auto& kv : why) std::printf("   %-22s %d\n", kv.first.c_str(), kv.second);
  return 0;
}
