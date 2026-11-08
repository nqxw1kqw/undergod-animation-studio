#include "export.hpp"
#include "../third_party/nlohmann/json.hpp"
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace studio {
using Json=nlohmann::json;
namespace {
void WriteJson(const std::string& path,const Json& j){
 std::ofstream f(path);f<<j.dump(2)<<'\n';f.close();if(!f)throw std::runtime_error("Cannot write "+path);
}
void RequireClip(const Clip& c){
 if(c.frames.empty())throw std::runtime_error("Cannot export empty clip");
 for(const auto& f:c.frames)if(!std::isfinite(f.duration)||f.duration<=0)throw std::runtime_error("Invalid export duration");
 if(!std::isfinite(c.Duration()))throw std::runtime_error("Invalid export length");
}
}
void ExportTrace(const Clip& c,const std::string& path){
 RequireClip(c);Json samples=Json::array();double t=0,previous=-1;
 for(size_t i=0;i<=c.frames.size();++i){
  auto s=SampleInterval(c,previous,t,false);const auto& p=s.pose;Json cues=Json::array();
  for(const auto& f:s.fired){const auto& cue=c.cues.at(f.index);cues.push_back({{"index",f.index},{"cycle",f.cycle},{"time_ms",f.at},{"action",cue.action},{"duration_ms",cue.duration},{"value",cue.value},{"text",cue.text}});}
  samples.push_back({{"time_ms",t},{"frame_index",p.frame},{"frame_file",c.frames.at(p.frame).file},
   {"pose",{{"x",p.x},{"y",p.y},{"scale",p.scale},{"rotation",p.rotation},{"r",p.r},{"g",p.g},{"b",p.b},{"alpha",p.alpha},{"desaturate",p.desaturate}}},{"cues",cues}});
  previous=t;if(i<c.frames.size())t+=c.frames[i].duration;
 }
 WriteJson(path,{{"version",1},{"actor",c.actor},{"clip_id",c.id},{"loop",false},{"samples",samples}});
}
void ExportContactSheet(const Clip& c,const FrameDecoder& decode,const std::string& bmpPath,const std::string& jsonPath,bool strip){
 RequireClip(c);auto first=decode(c.frames.front().file);
 const int cw=first.width,ch=first.height;
 if(cw<=0||ch<=0)throw std::runtime_error("Invalid frame dimensions");
 const size_t columns=strip?1:static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(c.frames.size()))));
 const size_t rows=(c.frames.size()-1)/columns+1;
 const std::uint64_t w=columns*static_cast<std::uint64_t>(cw),h=rows*static_cast<std::uint64_t>(ch);
 if(w>2147483647||h>2147483647||w*h>(std::numeric_limits<std::uint32_t>::max()-122ull)/4)throw std::runtime_error("Sheet too large");
 std::vector<std::uint8_t> pixels(static_cast<size_t>(w*h*4),0);Json frames=Json::array();
 for(size_t i=0;i<c.frames.size();++i){
  auto image=i==0?std::move(first):decode(c.frames[i].file);
  if(image.width!=cw||image.height!=ch||image.rgba.size()!=static_cast<size_t>(cw)*ch*4)throw std::runtime_error("Frame size mismatch: "+c.frames[i].file);
  const size_t x=i%columns*cw,y=i/columns*ch;
  for(int py=0;py<ch;++py)for(int px=0;px<cw;++px){
   size_t src=(static_cast<size_t>(py)*cw+px)*4,dst=((h-1-(y+py))*w+x+px)*4;
   pixels[dst]=image.rgba[src+2];pixels[dst+1]=image.rgba[src+1];pixels[dst+2]=image.rgba[src];pixels[dst+3]=image.rgba[src+3];
  }
  frames.push_back({{"index",i},{"file",c.frames[i].file},{"duration_ms",c.frames[i].duration},{"rect",{{"x",x},{"y",y},{"width",cw},{"height",ch}}}});
 }
 std::ofstream out(bmpPath,std::ios::binary);
 auto le=[&](std::uint32_t v,int n=4){for(int i=0;i<n;++i)out.put(static_cast<char>((v>>(8*i))&255));};
 out.write("BM",2);le(static_cast<std::uint32_t>(122+pixels.size()));le(0);le(122);
 le(108);le(static_cast<std::uint32_t>(w));le(static_cast<std::uint32_t>(h));le(1,2);le(32,2);le(3);le(static_cast<std::uint32_t>(pixels.size()));le(2835);le(2835);le(0);le(0);
 le(0x00ff0000);le(0x0000ff00);le(0x000000ff);le(0xff000000);le(0x73524742);for(int i=0;i<12;++i)le(0);
 out.write(reinterpret_cast<const char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));out.close();if(!out)throw std::runtime_error("Cannot write "+bmpPath);
 WriteJson(jsonPath,{{"version",1},{"clip_id",c.id},{"actor",c.actor},{"frame_count",c.frames.size()},{"cell_size",{{"width",cw},{"height",ch}}},{"grid",{{"columns",columns},{"rows",rows}}},{"sheet_size",{{"width",w},{"height",h}}},{"frames",frames}});
}
}
