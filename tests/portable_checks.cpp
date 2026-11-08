#include "animator.hpp"
#include "../third_party/nlohmann/json.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <chrono>
using namespace studio;
namespace fs=std::filesystem;
using Json=nlohmann::json;
void Check(bool b,const std::string& message){if(!b)throw std::runtime_error(message);}
bool Near(double a,double b,double e=1e-8){return std::abs(a-b)<e;}
bool Same(const Pose& a,const Pose& b){return a.frame==b.frame&&Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.scale,b.scale)&&Near(a.rotation,b.rotation)&&Near(a.r,b.r)&&Near(a.g,b.g)&&Near(a.b,b.b)&&Near(a.alpha,b.alpha)&&Near(a.desaturate,b.desaturate);}
void Result(const char* s){std::cout<<"RESULT "<<s<<" PASS\n";}
struct Temp {
 fs::path path=fs::temp_directory_path()/("undergod-studio-check-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Temp(){fs::create_directories(path/"data");fs::copy("assets",path/"assets",fs::copy_options::recursive);fs::copy("data",path/"data",fs::copy_options::recursive|fs::copy_options::overwrite_existing);}
 ~Temp(){std::error_code e;fs::remove_all(path,e);}
};
int main(){try{
 auto lib=LoadLibrary();size_t count=0;std::set<std::string> unique;
 Check(lib.actors.at("chick").size()==8&&lib.actors.at("frog").size()==7,"Actor clip inventory");
 for(const auto& actor:lib.actors)for(const auto& entry:actor.second){const auto& c=entry.second;++count;
  Check(c.frames.size()==4,"Four frames per delivered clip");
  for(size_t i=0;i<4;++i){const auto& f=c.frames[i];Check(fs::exists(f.file)&&f.duration>0,"Frame exists and duration positive");Check(f.file.substr(f.file.size()-6)=="_"+std::to_string(i)+".png","Frame index in range/order");unique.insert(f.file);}
  Check(c.composed==(c.id=="defeat"||c.id=="victory"),"Composed marker");
 }
 Check(count==15&&unique.size()==44,"15 clips reference exactly 44 source character frames");Result("clip_inventory_frames_durations");
 for(const auto& actor:lib.actors)for(const auto& entry:actor.second){const auto& c=entry.second;
  for(double t:{0.,1.,124.9,250.,320.,500.,650.,800.,1000.,3000.}){
   const auto before=SamplePose(c,t,c.loop);SamplePose(c,17,c.loop);Check(Same(before,SamplePose(c,t,c.loop)),"Pure sample determinism");Check(before.frame<4,"Sample frame in range");
  }
 }
 Result("sampler_determinism");
 for(const auto& actor:lib.actors)for(const auto& entry:actor.second){const auto& c=entry.second;
  for(bool loop:{false,true}){
   const double end=c.Duration()*3+100;auto expected=SampleInterval(c,-1,end,loop);Player p;std::vector<Fired> actual;
   for(double t=0;t<end;t+=17){auto s=p.Advance(c,t,loop);actual.insert(actual.end(),s.fired.begin(),s.fired.end());Check(p.Advance(c,t,loop).fired.empty(),"Repeated advance emits nothing");}
   auto tail=p.Advance(c,end,loop);actual.insert(actual.end(),tail.fired.begin(),tail.fired.end());Check(actual.size()==expected.fired.size(),"Partition independent cue count");
   std::set<std::pair<long long,size_t>> ids;
   for(size_t i=0;i<actual.size();++i){Check(actual[i].at==expected.fired[i].at&&actual[i].index==expected.fired[i].index&&actual[i].cycle==expected.fired[i].cycle,"Cue order stable");Check(ids.insert({actual[i].cycle,actual[i].index}).second,"Exactly once per cycle");}
   p.Seek(100);Check(p.Advance(c,100,loop).fired.empty(),"Seek does not refire");p.Restart();Check(p.Advance(c,0,loop).fired.size()==SampleInterval(c,-1,0,loop).fired.size(),"Restart includes zero cues");
  }
 }
 // Explicit simultaneous and wrap-boundary ordering independent of production data.
 Clip synthetic;synthetic.frames={{"unused",100}};synthetic.cues={{0,1,1,"shake",""},{0,1,1,"flash",""},{99,1,1,"popup",""}};
 auto fired=SampleInterval(synthetic,-1,200,true).fired;
 Check(fired.size()==8&&fired[0].index==0&&fired[1].index==1&&fired[2].at==99&&fired[3].at==100&&fired[6].at==200,"Zero and simultaneous cues across two wraps");
 Result("cue_exactly_once_ordering_and_large_dt");
 for(const auto& actor:lib.actors)for(const auto& entry:actor.second){const auto& c=entry.second;if(!c.loop)continue;
  Check(Same(SamplePose(c,0,true),SamplePose(c,c.Duration(),true)),"Exact loop seam");
  Check(Same(SamplePose(c,137,true),SamplePose(c,c.Duration()*4+137,true)),"Loop periodicity");
  auto a=SamplePose(c,c.Duration()-.0001,true),b=SamplePose(c,0,true);
  Check(Near(a.x,b.x,.001)&&Near(a.y,b.y,.001)&&Near(a.scale,b.scale,.001),"Continuous transform at seam");
  Check(SamplePose(c,c.Duration()-.0001,true).frame==3&&b.frame==0,"Frame wraps last to first");
 }
 Result("seamless_loop_wrap");
 using undergod::Cue;
 std::set<Cue> coverage;for(const auto& s:lib.cinematic.steps)coverage.insert(s.cue);
 Check(coverage==std::set<Cue>{Cue::Dim,Cue::Zoom,Cue::Pose,Cue::Vfx,Cue::Spear,Cue::Shake,Cue::Impact,Cue::Flash,Cue::End},"Every cinematic action parsed");
 int impacts=0,ends=0;std::vector<Cue> cues;undergod::TimelinePlayer tp;
 tp.Start(lib.cinematic,[&](const undergod::Step& s){cues.push_back(s.cue);},[&]{++impacts;},[&]{++ends;});tp.Advance(10000);tp.Advance(10000);tp.Skip();
 Check(impacts==1&&ends==1&&cues.size()==lib.cinematic.steps.size()-2,"All cinematic actions dispatched once");
 impacts=ends=0;tp.Start(lib.cinematic,[](const auto&){},[&]{++impacts;},[&]{++ends;});tp.Skip();tp.Skip();Check(impacts==1&&ends==1&&!tp.Active(),"Skip once");
 auto p=SampleCinematic(lib,500);Check(Near(p.dim,.75)&&Near(p.zoom,1.12,1e-6)&&p.playerClip=="ult_throw","Dim zoom pose");
 p=SampleCinematic(lib,1100);Check(p.spearVisible&&!p.falling&&Near(p.spearX,633.75)&&Near(p.spearY,-137.5),"Ease-out spear rise");
 p=SampleCinematic(lib,2025);Check(p.falling&&Near(p.spearY,7.5),"Quadratic spear fall");
 p=SampleCinematic(lib,2600);Check(p.impacted&&Near(p.vfxAge,0)&&p.vfxFrames==4&&p.vfxEnemy,"Impact/VFX");
 Check(std::abs(SampleCinematic(lib,2620).shake)>0&&SampleCinematic(lib,2700).flash>0,"Shake/flash");
 p=SampleCinematic(lib,3275);Check(Near(p.zoom,1.03,1e-6),"Return zoom at boundary");
 p=SampleCinematic(lib,3600);Check(p.ended&&p.impacted&&p.dim==0&&p.zoom==1&&!p.spearVisible,"End clears visuals");
 for(double t:{0.,750.,1450.,2600.,3000.,3600.}){auto a=SampleCinematic(lib,t);SampleCinematic(lib,3555);auto b=SampleCinematic(lib,t);Check(Same(a.player,b.player)&&a.zoom==b.zoom&&a.spearY==b.spearY,"Cinematic seek determinism");}
 Result("cinematic_all_9_actions_sampling_and_skip");
 auto defeat=SamplePose(lib.actors.at("chick").at("defeat"),800,false);Check(Near(defeat.alpha,.15)&&Near(defeat.rotation,.65)&&defeat.desaturate==1,"Defeat treatment");
 Check(SamplePose(lib.actors.at("frog").at("victory"),200,false).y<-17,"Victory bounce");Result("composed_terminal_tracks");
 Temp tmp;Json original;{std::ifstream f("data/clips.json");f>>original;}
 auto write=[&](const Json& j){std::ofstream f(tmp.path/"data/clips.json");f<<j.dump();};
 std::string error;auto reject=[&]{Check(!Reload(lib,error,tmp.path.string())&&!error.empty(),"Invalid data rejected");Check(lib.actors.at("chick").size()==8&&lib.cinematic.steps.size()==11,"Previous library preserved");};
 {std::ofstream f(tmp.path/"data/clips.json");f<<"{broken";}reject();
 for(int kind=0;kind<6;++kind){auto j=original;auto& c=j["actors"]["chick"]["idle"];
  if(kind==0)c["frames"][0]["duration"]=0;
  if(kind==1)c["frames"][0]["file"]="assets/missing.png";
  if(kind==2)c["tracks"]["unknown"]=Json::array();
  if(kind==3)c["tracks"]["y"][2]["value"]=2;
  if(kind==4)c["cues"]=Json::array({{{"t",0},{"duration",100},{"value",1},{"action","unknown"}}});
  if(kind==5)c["frames"]=Json::array();
  write(j);reject();
 }
 write(original);Json cinema;{std::ifstream f("data/cinematic.json");f>>cinema;}
 auto broken=cinema;broken["steps"][0]["action"]="unknown";{std::ofstream f(tmp.path/"data/cinematic.json");f<<broken.dump();}reject();
 {std::ofstream f(tmp.path/"data/cinematic.json");f<<cinema.dump();}
 Check(Reload(lib,error,tmp.path.string())&&error.empty(),"Valid reload recovers");Result("transactional_reload_invalid_json_and_validation");
 return 0;
 }catch(const std::exception& e){std::cerr<<"RESULT portable FAIL: "<<e.what()<<'\n';return 1;}}
