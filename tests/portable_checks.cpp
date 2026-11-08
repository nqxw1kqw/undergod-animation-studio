#include "animator.hpp"
#include "export.hpp"
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
 {
 ActorControls controls;
 nlohmann::ordered_json catalog;{std::ifstream input("data/clips.json");input>>catalog;}
 for(const auto& who:{"chick","frog"}){
  controls.Apply(lib,{ActorAction::SelectActor,0,who});std::vector<std::string> expected;
  for(const auto& entry:catalog["actors"][who].items())if(!entry.value().value("composed",false))expected.push_back(entry.key());
  Check(controls.SlotClips(lib)==expected,"Slot list follows actual JSON order");
  for(size_t i=0;i<expected.size();++i)Check(controls.ResolveSlot(lib,int(i+1))==expected[i],"Slot resolves catalog clip");
 }
 Result("actor_slot_catalog_mapping");
 controls.Apply(lib,{ActorAction::SelectActor,0,"frog"});controls.time=321;controls.playing=false;
 for(int slot:{-1,0,6,7,99}){
  Check(!controls.Apply(lib,{ActorAction::AnimationSlot,slot,{}})&&controls.time==321&&controls.clip=="idle"&&!controls.playing&&controls.loop,"Invalid slot is full no-op");
 }
 Check(!controls.Apply(lib,{ActorAction::SelectActor,0,"missing"})&&controls.actor=="frog","Unknown actor no-op");
 Result("actor_out_of_range_noop");
 ActorControls cycle;Check(cycle.actor=="chick","Default active actor");
 cycle.Apply(lib,{ActorAction::SelectActor,0,"frog"});Check(cycle.actor=="frog"&&cycle.Clips(lib).size()==7,"Select frog clip list");
 cycle.Apply(lib,{ActorAction::SelectActor,0,"chick"});Check(cycle.actor=="chick"&&cycle.Clips(lib).size()==9,"Select chick clip list");
 cycle.Apply(lib,{ActorAction::AnimationSlot,6,{}});cycle.Apply(lib,{ActorAction::SelectActor,0,"frog"});
 Check(cycle.clip=="idle"&&cycle.time==0,"Actor select missing clip falls back to idle");
 cycle.Apply(lib,{ActorAction::AnimationSlot,2,{}});cycle.time=40;cycle.Apply(lib,{ActorAction::SelectActor,0,"chick"});
 Check(cycle.clip=="attack"&&cycle.time==0&&!cycle.loop,"Actor select retains available clip and restarts");
 auto editedLibrary=lib;EditFrameDuration(editedLibrary.actors.at("chick").at("idle"),0,10);
 cycle.Apply(editedLibrary,{ActorAction::SelectActor,0,"frog"});
 Check(editedLibrary.actors.at("chick").at("idle").frames[0].duration==260,"Switch retains unsaved edits");
 Result("actor_select_clip_list_and_edits");
 ActorControls initial;initial.AdvanceOthers(lib,25);Check(initial.Playback("frog").time==25,"Unselected actor advances from startup");
 ActorControls independent;independent.time=234;independent.Apply(lib,{ActorAction::SelectActor,0,"frog"});independent.time=456;
 auto other=independent.Playback("chick");independent.Apply(lib,{ActorAction::AnimationSlot,2,{}});
 Check(independent.clip=="attack"&&independent.time==0&&independent.playing&&!independent.loop,"Hotkey selects and starts with default mode");
 for(bool loop:{false,true}){independent.time=87;independent.loop=loop;independent.playing=false;
  independent.Apply(lib,{ActorAction::AnimationSlot,2,{}});
  Check(independent.time==0&&independent.playing&&independent.loop==loop,"Same clip restart preserves loop/once override");
 }
 auto unchanged=independent.Playback("chick");
 Check(unchanged.clip==other.clip&&unchanged.time==other.time&&unchanged.loop==other.loop&&unchanged.playing==other.playing,"Other player undisturbed");
 independent.AdvanceOthers(lib,20);Check(independent.Playback("chick").time==254,"Other transport advances independently");
 // Check isolation in the opposite direction with a paused, non-idle actor.
 independent.time=111;independent.playing=false;independent.Apply(lib,{ActorAction::SelectActor,0,"chick"});
 other=independent.Playback("frog");independent.Apply(lib,{ActorAction::AnimationSlot,3,{}});unchanged=independent.Playback("frog");
 Check(unchanged.clip==other.clip&&unchanged.time==111&&!unchanged.playing&&unchanged.loop==other.loop,"Chick hotkey preserves paused frog");
 // Undo/reload may remove a copied clip selected by an inactive actor.
 auto removed=lib;auto copied=DuplicateClip(removed,"frog","idle");
 independent.Switch(removed,"frog");independent.clip=copied;independent.Switch(removed,"chick");
 removed.actors.at("frog").erase(copied);independent.Reconcile(removed);
 Check(independent.Playback("frog").clip=="idle","Removed inactive clip safely falls back");
 Result("actor_hotkey_restart_isolation");
 independent.Apply(lib,{ActorAction::SelectActor,0,"frog"});auto before=independent.Playback("frog");
 Check(!independent.Apply(lib,{ActorAction::Cinematic,0,{}})&&independent.clip==before.clip&&independent.time==before.time&&independent.playing==before.playing,"Frog cinematic no-op");
 independent.Apply(lib,{ActorAction::SelectActor,0,"chick"});other=independent.Playback("frog");
 Check(independent.Apply(lib,{ActorAction::Cinematic,0,{}})&&independent.clip=="sao_roi"&&independent.time==0&&independent.playing&&!independent.loop,"Chick cinematic starts once");
 independent.time=100;independent.Apply(lib,{ActorAction::Cinematic,0,{}});Check(independent.time==0&&independent.Playback("frog").time==other.time,"Cinematic restarts without changing other transport");
 independent.Apply(lib,{ActorAction::SelectActor,0,"frog"});Check(independent.clip=="idle","Frog cannot inherit chick cinematic");
 Result("actor_cinematic_ownership");
 for(const auto& who:{"chick","frog"}){controls.Apply(lib,{ActorAction::SelectActor,0,who});auto help=controls.Help(lib);auto slots=controls.SlotClips(lib);
  for(size_t i=0;i<slots.size();++i)Check(help.find("F"+std::to_string(i+1)+" "+slots[i])!=std::string::npos,"Help matches catalog");
  Check((help.find("C sao_roi")!=std::string::npos)==(controls.actor=="chick")&&help.find("1/2 switch actor")!=std::string::npos&&help.find("Active: "+controls.actor)!=std::string::npos,"Help actor, ownership and switching");
  Check(help.find("[ ] switch")==std::string::npos,"Brackets stay cue nudges");
 }
 {Temp saved;auto copy=lib;std::string error;
  Check(SaveClips(copy,error,saved.path.string()),"Save ordered catalog: "+error);
  auto loaded=LoadLibrary(saved.path.string());
  for(const auto& who:{"chick","frog"}){controls.Apply(lib,{ActorAction::SelectActor,0,who});
   Check(controls.SlotClips(loaded)==controls.SlotClips(lib),"Hotkey order survives save/reload");}
 }
 Result("actor_help_matches_catalog");
 }
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
 // Editing primitives keep cue selection stable across sorting and clamp both bounds.
 Clip edit=lib.actors.at("chick").at("attack");
 edit.cues={{10,100,1,"shake","a"},{20,100,1,"flash","b"},{30,100,1,"popup","c"}};
 size_t selected=NudgeCue(edit,0,50);Check(selected==2&&edit.cues[2].text=="a"&&edit.cues[2].t==60,"Nudge reorders and follows selection");
 selected=NudgeCue(edit,selected,-1000);Check(selected==0&&edit.cues[0].t==0,"Cue lower clamp");
 selected=NudgeCue(edit,selected,10000);Check(selected==2&&edit.cues[2].t==edit.Duration(),"Cue inclusive upper clamp");
 double old=edit.Duration();EditFrameDuration(edit,0,-10000);Check(edit.frames[0].duration==20&&edit.Duration()<old&&edit.cues[2].t==edit.Duration(),"Frame floor and cue clamp");
 EditFrameDuration(edit,0,10);Check(edit.frames[0].duration==30&&edit.tracks.at("x").back().t==edit.Duration(),"Frame increase and track retiming");
 Result("timeline_nudge_clamp_resort_duration_edits");
 auto edited=LoadLibrary(tmp.path.string());edited.actors.at("chick").at("attack")=edit;
 EditFrameDuration(edited.actors.at("chick").at("idle"),0,10);
 auto bytes=[&](const fs::path& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
 const auto cinematicBytes=bytes(tmp.path/"data/cinematic.json");
 Check(SaveClips(edited,error,tmp.path.string()),"Save succeeds: "+error);
 auto reloaded=LoadLibrary(tmp.path.string());
 auto equal=[](const Library& a,const Library& b){
  Check(a.actors.size()==b.actors.size(),"Actor roundtrip");
  for(const auto& actor:a.actors){Check(actor.second.size()==b.actors.at(actor.first).size(),"Clip roundtrip");for(const auto& entry:actor.second){
   const auto& x=entry.second;const auto& y=b.actors.at(actor.first).at(entry.first);
   Check(x.actor==y.actor&&x.id==y.id&&x.loop==y.loop&&x.composed==y.composed&&x.frames.size()==y.frames.size()&&x.tracks.size()==y.tracks.size()&&x.cues.size()==y.cues.size(),"Clip fields roundtrip");
   for(size_t i=0;i<x.frames.size();++i)Check(x.frames[i].file==y.frames[i].file&&x.frames[i].duration==y.frames[i].duration,"Frames roundtrip");
   for(const auto& tr:x.tracks){const auto& keys=y.tracks.at(tr.first);Check(tr.second.size()==keys.size(),"Keys roundtrip");for(size_t i=0;i<keys.size();++i)Check(tr.second[i].t==keys[i].t&&tr.second[i].value==keys[i].value&&tr.second[i].ease==keys[i].ease,"Track roundtrip");}
   for(size_t i=0;i<x.cues.size();++i){const auto& q=x.cues[i];const auto& r=y.cues[i];Check(q.t==r.t&&q.duration==r.duration&&q.value==r.value&&q.action==r.action&&q.text==r.text,"Cues roundtrip");}
  }}
 };
 equal(edited,reloaded);Check(bytes(tmp.path/"data/cinematic.json")==cinematicBytes,"Cinematic untouched");
 Result("timeline_save_reload_roundtrip_equality");
 auto structural=edited;auto& sc=structural.actors.at("chick").at("attack");
 const auto source=sc.frames[0];const auto size=sc.frames.size();
 Check(InsertFrame(sc,0)==1&&sc.frames.size()==size+1&&sc.frames[1].file==source.file&&sc.frames[1].duration==source.duration,"Insert copies selected frame");
 sc.cues.push_back({sc.Duration(),100,1,"shake","end"});
 Check(DeleteFrame(sc,1)&&sc.frames.size()==size&&sc.cues.back().t==sc.Duration(),"Delete repairs end cue");
 auto copyId=DuplicateClip(structural,"chick","attack");Check(copyId=="attack_copy"&&DuplicateClip(structural,"chick","attack")=="attack_copy2","Unused copy suffix");
 auto expectedCopy=sc;expectedCopy.id=copyId;Library expected=structural;expected.actors.at("chick").at(copyId)=expectedCopy;equal(structural,expected);
 auto minimumClip=sc;while(minimumClip.frames.size()>1)Check(DeleteFrame(minimumClip,0),"Delete down to one");
 const auto last=minimumClip.frames[0];Check(!DeleteFrame(minimumClip,0)&&minimumClip.frames.size()==1&&minimumClip.frames[0].file==last.file&&minimumClip.frames[0].duration==last.duration,"Last frame refusal unchanged");
 Result("structural_insert_delete_duplicate_minimum_and_cue_repair");
 Check(SaveClips(structural,error,tmp.path.string()),"Structural save: "+error);equal(structural,LoadLibrary(tmp.path.string()));
 Result("structural_save_reload_roundtrip");
 Check(SaveClips(edited,error,tmp.path.string()),"Restore edited fixture");
 EditHistory history;auto working=edited;const auto initial=working;
 auto change=[&](bool cinematic,auto operation){auto before=working;operation();history.Record(before,working,cinematic);};
 change(false,[&]{NudgeCue(working.actors.at("chick").at("attack"),0,10);});
 change(false,[&]{EditFrameDuration(working.actors.at("chick").at("idle"),0,10);});
 change(true,[&]{NudgeStep(working.cinematic,2,10);});
 change(false,[&]{InsertFrame(working.actors.at("chick").at("attack"),0);});
 change(false,[&]{DeleteFrame(working.actors.at("chick").at("attack"),2);});
 change(false,[&]{DuplicateClip(working,"chick","idle");});
 const auto finalState=working;Check(history.UndoCount()==6,"Mixed history size");
 while(history.Undo(working)){}
 equal(initial,working);Check(SameDocument(initial,working,true),"Undo restores exact cinematic");Check(!history.Undo(working),"Empty undo");
 while(history.Redo(working)){}
 equal(finalState,working);Check(SameDocument(finalState,working,true),"Redo restores exact cinematic");
 Check(history.Undo(working),"Undo before branch");change(false,[&]{EditFrameDuration(working.actors.at("chick").at("idle"),0,10);});
 Check(!history.Redo(working),"New edit clears redo");
 auto noOp=working;history.Record(noOp,working,false);Check(history.UndoCount()==6,"No-op omitted");
 Result("history_mixed_edits_exact_undo_redo_and_branch");
 history.Clear();auto boundStart=working;
 for(size_t i=0;i<EditHistory::Limit+5;++i)change(false,[&]{EditFrameDuration(working.actors.at("chick").at("idle"),0,10);});
 Check(history.UndoCount()==50,"History bound");size_t undone=0;while(history.Undo(working))++undone;
 Check(undone==50&&working.actors.at("chick").at("idle").frames[0].duration==boundStart.actors.at("chick").at("idle").frames[0].duration+50,"Oldest five evicted");
 size_t redone=0;while(history.Redo(working))++redone;Check(redone==50,"Bounded redo");
 // Undo restores only the affected file, preserving independent changes.
 history.Clear();auto beforeOnly=working;EditFrameDuration(working.actors.at("chick").at("idle"),0,10);history.Record(beforeOnly,working,false);
 NudgeStep(working.cinematic,2,10);auto independent=working;Check(history.Undo(working)&&SameDocument(working,independent,true),"Affected file isolation");
 history.Clear();Check(!history.Undo(working)&&!history.Redo(working),"Reload clear semantics");
 Result("history_bound_and_affected_file_isolation");
 const auto saved=bytes(tmp.path/"data/clips.json");
 for(int kind=0;kind<5;++kind){auto invalid=edited;auto& c=invalid.actors.at("frog").at("idle");
  if(kind==0)c.frames[0].duration=19;
  if(kind==1)c.frames[0].file="assets/missing.png";
  if(kind==2)c.tracks["y"].back().t=c.Duration()+1;
  if(kind==3)c.cues.push_back({0,10,1,"unknown",""});
  if(kind==4)c.tracks["empty"]={};
  Check(!SaveClips(invalid,error,tmp.path.string())&&!error.empty(),"Invalid complete catalog rejected");
  Check(bytes(tmp.path/"data/clips.json")==saved,"Failed save leaves bytes identical");
 }
 Result("timeline_invalid_save_preserves_file_bytes");
 for(const auto& actor:lib.actors)for(const auto& entry:actor.second){const auto& c=entry.second;
  for(bool loop:{false,true})for(size_t i=0;i<c.frames.size();++i){auto n=NeighborFrames(c,i,loop);
   Check(n.previous==(i?i-1:(loop?c.frames.size()-1:0)),"Previous neighbor");
   Check(n.next==(i+1<c.frames.size()?i+1:(loop?0:i)),"Next neighbor");
  }
 }
 Clip single;single.frames={{"unused",20}};for(bool loop:{false,true}){auto n=NeighborFrames(single,0,loop);Check(n.previous==0&&n.next==0,"Single-frame neighbors");}
 bool rejected=false;try{NeighborFrames(single,1,true);}catch(const std::exception&){rejected=true;}Check(rejected,"Invalid neighbor index rejected");
 Result("onion_skin_loop_aware_frame_neighbors");
 for(const auto& a:lib.actors)for(const auto& name:{"attack","ult_charge","ult_throw"})if(a.second.count(name)){
  const auto& c=a.second.at(name);double impact=ImpactTime(c);bool found=false;for(const auto& q:c.cues)if(q.action=="impact"){Check(impact==q.t,"Cue offset");found=true;}Check(found,"Delivered attacks have impact");
  Check(TargetReaction(lib,"frog",impact-1,impact).clip=="idle"&&TargetReaction(lib,"frog",impact,impact).clip=="hit","Reaction boundary");
  Check(TargetReaction(lib,"frog",ExchangeDuration(lib,a.first,"frog",name),impact).clip=="idle","Exchange returns idle");
 }
 Check(ImpactTime(single)==20,"Missing cue defaults to end");
 Check(ImpactTime(lib.cinematic)==2600&&TargetReaction(lib,"frog",3100,2600,true).clip=="defeat","Cinematic hit then defeat");
 Result("dual_actor_impact_offsets_and_exchange");
 auto timeline=lib.cinematic;auto sel=NudgeStep(timeline,2,-10000);Check(timeline.steps[sel].t==0&&timeline.steps[sel].cue==Cue::Pose,"Step lower clamp and identity");
 sel=NudgeStep(timeline,sel,10000);Check(timeline.steps[sel].t==3600&&timeline.steps[sel].cue==Cue::Pose,"Step upper clamp and identity");
 Result("cinematic_nudge_clamp_selection");
 auto ce=LoadLibrary(tmp.path.string());NudgeStep(ce.cinematic,2,10);
 Check(SaveCinematic(ce,error,tmp.path.string()),"Cinematic save: "+error);
 auto cr=LoadLibrary(tmp.path.string());Check(undergod::SerializeTimeline(cr.cinematic)==undergod::SerializeTimeline(ce.cinematic),"All cinematic fields roundtrip");
 Result("cinematic_save_reload_roundtrip");
 const auto cb=bytes(tmp.path/"data/cinematic.json");
 for(int kind=0;kind<4;++kind){auto bad=ce;
  if(kind==0)for(auto& q:bad.cinematic.steps)if(q.cue==Cue::Impact)q.t+=10;
  if(kind==1)bad.cinematic.steps.back().t=0;
  if(kind==2)bad.cinematic.steps[0].value=2;
  if(kind==3)bad.cinematic.steps[0].t=std::nanf("");
  Check(!SaveCinematic(bad,error,tmp.path.string())&&!error.empty(),"Invalid cinematic save refused");Check(bytes(tmp.path/"data/cinematic.json")==cb,"Cinematic failure byte preservation");
 }
 Result("cinematic_invalid_save_preserves_file_bytes");
 double transport=0;Check(!AdvanceTransport(transport,5000,650,false)&&transport==650,"Once clamps and stops on last pose");
 Check(SamplePose(lib.actors.at("chick").at("attack"),transport,false).frame==3,"Once last frame");
 transport=0;Check(AdvanceTransport(transport,2000,650,true)&&transport==2000,"Loop retains cycle time");
 Check(NextSpeed(1)==1.25&&NextSpeed(3)==3&&NextSpeed(.25)==.5,"Shuttle speed steps and cap");
 Result("transport_once_freeze_loop_and_shuttle");
 // Export each delivered clip, including looped clips as one clamped pass.
 for(const auto& actor:lib.actors)for(const auto& entry:actor.second){
  const auto& c=entry.second;auto path=tmp.path/"trace.json";ExportTrace(c,path.string());
  Json trace;{std::ifstream f(path);f>>trace;}double previous=-1,expectedTime=0;
  Check(trace["samples"].size()==c.frames.size()+1,"All frame boundaries plus end");
  size_t sampleIndex=0;
  for(const auto& row:trace["samples"]){double t=row["time_ms"];Check(t>previous&&Near(t,expectedTime),"Monotonic exact frame boundaries");
   const auto expected=SampleInterval(c,previous,t,false);const auto& v=row["pose"];
   Pose actual;actual.frame=row["frame_index"];actual.x=v["x"];actual.y=v["y"];actual.scale=v["scale"];actual.rotation=v["rotation"];actual.r=v["r"];actual.g=v["g"];actual.b=v["b"];actual.alpha=v["alpha"];actual.desaturate=v["desaturate"];
   Check(Same(actual,expected.pose)&&row["frame_file"]==c.frames[actual.frame].file,"Trace equals playback sampler");
   Check(row["cues"].size()==expected.fired.size(),"Trace cue interval count");
   for(size_t i=0;i<expected.fired.size();++i)Check(row["cues"][i]["index"]==expected.fired[i].index&&row["cues"][i]["time_ms"]==expected.fired[i].at,"Trace cue identities");
   previous=t;if(sampleIndex<c.frames.size())expectedTime+=c.frames[sampleIndex++].duration;
  }
 }
 Result("export_trace_boundaries_sampler_and_cues");
 Clip sheetClip;sheetClip.id="test";sheetClip.frames={{"0",20},{"1",30},{"2",40},{"3",50}};
 auto decode=[](const std::string& file){ExportImage image;image.width=3;image.height=2;for(int y=0;y<2;++y)for(int x=0;x<3;++x){image.rgba.insert(image.rgba.end(),{static_cast<unsigned char>(10+std::stoi(file)),static_cast<unsigned char>(y),static_cast<unsigned char>(x),123});}return image;};
 for(bool strip:{false,true}){
  auto bmp=tmp.path/"sheet.bmp",sidecar=tmp.path/"sheet.json";
  ExportContactSheet(sheetClip,decode,bmp.string(),sidecar.string(),strip);
  std::ifstream f(bmp,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});
  auto le=[&](size_t at){return unsigned(bytes.at(at))|(unsigned(bytes.at(at+1))<<8)|(unsigned(bytes.at(at+2))<<16)|(unsigned(bytes.at(at+3))<<24);};
  const unsigned w=strip?3:6,h=strip?8:4;
  Check(bytes[0]=='B'&&bytes[1]=='M'&&le(2)==bytes.size()&&le(10)==122&&le(14)==108,"BMP V4 header");
  Check(le(18)==w&&le(22)==h&&bytes[26]==1&&bytes[28]==32&&le(30)==3&&le(34)==w*h*4&&bytes.size()==122+w*h*4,"BMP dimensions and byte size");
  Check(le(54)==0xff0000&&le(66)==0xff000000,"BMP RGBA masks");
  Json meta;{std::ifstream in(sidecar);in>>meta;}Check(meta["clip_id"]=="test"&&meta["frame_count"]==4&&meta["cell_size"]["width"]==3&&meta["cell_size"]["height"]==2,"Sidecar identity and cell size");
  Check(meta["grid"]["columns"]==(strip?1:2)&&meta["grid"]["rows"]==(strip?4:2),"Sidecar grid");
  std::vector<int> covered(w*h,0);
  for(size_t i=0;i<4;++i){auto rect=meta["frames"][i]["rect"];unsigned rx=rect["x"],ry=rect["y"];
   Check(rect["width"]==3&&rect["height"]==2&&meta["frames"][i]["duration_ms"]==sheetClip.frames[i].duration,"Frame metadata");
   for(unsigned y=0;y<2;++y)for(unsigned x=0;x<3;++x){Check(rx+x<w&&ry+y<h,"Rect inside sheet");++covered[(ry+y)*w+rx+x];size_t at=122+((h-1-(ry+y))*w+rx+x)*4;
    Check(bytes[at]==x&&bytes[at+1]==y&&bytes[at+2]==10+i&&bytes[at+3]==123,"Ordered source pixels, orientation and alpha");}
  }
  for(int n:covered)Check(n==1,"Rects tile sheet exactly once");
 }
 Result("export_contact_sheet_bmp_header_pixels_and_rects");
 // A partial last row remains transparent; mismatched images and I/O fail visibly.
 sheetClip.frames.pop_back();ExportContactSheet(sheetClip,decode,(tmp.path/"partial.bmp").string(),(tmp.path/"partial.json").string());
 {std::ifstream f(tmp.path/"partial.bmp",std::ios::binary);f.seekg(122+3*4);char pixel[4];f.read(pixel,4);Check(pixel[0]==0&&pixel[1]==0&&pixel[2]==0&&pixel[3]==0,"Unused cell transparent");}
 bool failed=false;try{ExportContactSheet(sheetClip,[](const std::string&){return ExportImage{};},(tmp.path/"bad.bmp").string(),(tmp.path/"bad.json").string());}catch(const std::exception&){failed=true;}Check(failed,"Invalid dimensions rejected");
 failed=false;try{ExportTrace(sheetClip,(tmp.path/"missing"/"trace.json").string());}catch(const std::exception&){failed=true;}Check(failed,"Write failure reported");
 Result("export_partial_grid_and_errors");
 return 0;
 }catch(const std::exception& e){std::cerr<<"RESULT portable FAIL: "<<e.what()<<'\n';return 1;}}
