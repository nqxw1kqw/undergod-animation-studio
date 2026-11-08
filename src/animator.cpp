#include "animator.hpp"
#include "../third_party/nlohmann/json.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace studio {
namespace {
using Json=nlohmann::json;
void Require(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
double Number(const Json& j){double v=j.get<double>();Require(std::isfinite(v),"Nonfinite number");return v;}
double Ease(double u,const std::string& ease){
    if(ease=="smooth")return u*u*(3-2*u); // Game::CardVisual / ActorOffset
    if(ease=="inQuad")return u*u;
    if(ease=="outQuad")return 1-(1-u)*(1-u); // Game::SampleZoom / SpearTip
    return u;
}
double Track(const std::vector<Key>& keys,double t){
    if(t<=keys.front().t)return keys.front().value;
    for(size_t i=1;i<keys.size();++i)if(t<keys[i].t){const auto& a=keys[i-1];const auto& b=keys[i];return a.value+(b.value-a.value)*Ease((t-a.t)/(b.t-a.t),b.ease);}
    return keys.back().value;
}
void File(const std::string& root,const std::string& path){
    const std::filesystem::path p(path);
    Require(!p.is_absolute()&&path.find("..") == std::string::npos,"Unsafe asset path: "+path);
    Require(std::filesystem::is_regular_file(std::filesystem::path(root)/p),"Missing asset: "+path);
}
}
double Clip::Duration()const{double d=0;for(const auto& f:frames)d+=f.duration;return d;}
static Library ParseLibrary(const Json& j,const std::string& root){
    Require(j.at("version")==1,"Unsupported clips version");Library lib;
    const std::set<std::string> trackNames={"x","y","scale","rotation","r","g","b","alpha","desaturate"};
    const std::set<std::string> cueNames={"shake","flash","particles","popup","dim","zoom","impact"};
    for(const auto& actor:j.at("actors").items()){
        Require(!actor.key().empty(),"Empty actor");
        for(const auto& entry:actor.value().items()){
            Clip c;c.actor=actor.key();c.id=entry.key();const auto& v=entry.value();c.loop=v.at("loop");c.composed=v.value("composed",false);
            for(const auto& frame:v.at("frames")){
                Frame fr{frame.at("file"),Number(frame.at("duration"))};Require(fr.duration>0,"Frame duration must be positive");File(root,fr.file);c.frames.push_back(fr);
            }
            Require(!c.frames.empty(),"Empty clip: "+c.id);const double duration=c.Duration();Require(std::isfinite(duration),"Invalid clip duration");
            const auto tracks=v.value("tracks",Json::object());
            for(const auto& tr:tracks.items()){
                Require(trackNames.count(tr.key())!=0,"Unknown track: "+tr.key());auto& keys=c.tracks[tr.key()];
                for(const auto& k:tr.value()){
                    Key key{Number(k.at("t")),Number(k.at("value")),k.value("ease","linear")};
                    Require(key.t>=0&&key.t<=duration&&(keys.empty()||key.t>keys.back().t),"Invalid key time");
                    Require(key.ease=="linear"||key.ease=="smooth"||key.ease=="inQuad"||key.ease=="outQuad","Unknown easing");
                    if(tr.key()=="scale")Require(key.value>0,"Scale must be positive");
                    if(tr.key()=="alpha"||tr.key()=="desaturate")Require(key.value>=0&&key.value<=1,"Normalized track out of range");
                    if(tr.key()=="r"||tr.key()=="g"||tr.key()=="b")Require(key.value>=0&&key.value<=255,"Tint out of range");
                    keys.push_back(key);
                }
                Require(!keys.empty(),"Empty track");
                if(c.loop)Require(keys.front().t==0&&keys.back().t==duration&&keys.front().value==keys.back().value,"Loop track endpoints must match");
            }
            for(const auto& q:v.value("cues",Json::array())){
                Cue cue{Number(q.at("t")),Number(q.at("duration")),Number(q.at("value")),q.at("action"),q.value("text","")};
                Require(cueNames.count(cue.action)&&cue.t>=0&&cue.t<=duration&&cue.duration>0&&cue.value>=0,"Invalid effect cue");c.cues.push_back(cue);
            }
            std::stable_sort(c.cues.begin(),c.cues.end(),[](const auto& a,const auto& b){return a.t<b.t;});
            lib.actors[c.actor].emplace(c.id,std::move(c));
        }
        Require(!lib.actors[actor.key()].empty(),"Actor has no clips");
    }
    Require(lib.actors.count("chick")&&lib.actors.count("frog"),"Required actors missing");
    lib.cinematic=undergod::LoadTimeline(root+"/data/cinematic.json");
    for(const auto& s:lib.cinematic.steps){
        Require(std::isfinite(s.t)&&std::isfinite(s.duration)&&std::isfinite(s.value),"Invalid cinematic number");
        if(s.cue==undergod::Cue::Pose)Require(lib.actors.at(s.enemy?"frog":"chick").count(s.pose),"Cinematic references missing clip");
    }
    for(const auto* a:{"chick","frog"})Require(lib.actors.at(a).count("idle"),"Idle required");
    File(root,"assets/pastel/vfx/spear.png");File(root,"assets/pastel/vfx/spear_impact.png");
    return lib;
}
Library LoadLibrary(const std::string& root){
    std::ifstream f(root+"/data/clips.json");Require(bool(f),"Cannot open data/clips.json");Json j;f>>j;
    return ParseLibrary(j,root);
}
size_t NudgeStep(undergod::Timeline& t,size_t index,double delta){
    Require(std::isfinite(delta),"Invalid step delta");const double end=CinematicDuration(t);
    t.steps.at(index).t=float(std::clamp(double(t.steps.at(index).t)+delta,0.,end));
    std::vector<size_t> order;for(size_t i=0;i<t.steps.size();++i)order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return t.steps[a].t<t.steps[b].t;});
    auto old=t.steps;size_t selected=0;for(size_t i=0;i<order.size();++i){t.steps[i]=old[order[i]];if(order[i]==index)selected=i;}return selected;
}
bool SaveCinematic(Library& current,std::string& error,const std::string& root){
    const auto target=std::filesystem::path(root)/"data/cinematic.json",temp=std::filesystem::path(root)/"data/cinematic.json.tmp";bool created=false;
    try{
        const auto text=undergod::SerializeTimeline(current.cinematic);auto validated=undergod::ParseTimelineText(text);
        for(const auto& s:validated.steps)if(s.cue==undergod::Cue::Pose)Require(current.actors.at(s.enemy?"frog":"chick").count(s.pose),"Missing cinematic clip");
        Require(!std::filesystem::exists(temp),"Save temporary file already exists");
        {std::ofstream out(temp,std::ios::binary);created=true;Require(bool(out),"Cannot create temporary file");out<<text;out.flush();Require(bool(out),"Cannot write temporary file");out.close();Require(bool(out),"Cannot close temporary file");}
#ifdef _WIN32
        Require(MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"Cannot replace cinematic.json");
#else
        Require(std::rename(temp.c_str(),target.c_str())==0,"Cannot replace cinematic.json");
#endif
        current.cinematic=std::move(validated);error.clear();return true;
    }catch(const std::exception& e){if(created){std::error_code ignored;std::filesystem::remove(temp,ignored);}error=e.what();return false;}
}
size_t NudgeCue(Clip& c,size_t index,double delta){
    Require(std::isfinite(delta),"Invalid cue delta");
    c.cues.at(index).t=std::clamp(c.cues.at(index).t+delta,0.,c.Duration());
    // Sort identities as well as values so selection follows the edited cue.
    std::vector<size_t> order;for(size_t i=0;i<c.cues.size();++i)order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return c.cues[a].t<c.cues[b].t;});
    auto old=c.cues;size_t selected=0;
    for(size_t i=0;i<order.size();++i){c.cues[i]=old[order[i]];if(order[i]==index)selected=i;}
    return selected;
}
void EditFrameDuration(Clip& c,size_t index,double delta){
    Require(std::isfinite(delta),"Invalid frame delta");const double before=c.Duration();
    auto& f=c.frames.at(index);f.duration=std::max(20.,f.duration+delta);
    const double after=c.Duration();
    for(auto& track:c.tracks)for(auto& key:track.second)key.t=key.t==before?after:key.t*(after/before);
    for(auto& cue:c.cues)cue.t=std::clamp(cue.t,0.,after);
}
namespace {
void RepairLength(Clip& c,double before){
    const double after=c.Duration();
    for(auto& track:c.tracks)for(auto& key:track.second)key.t=key.t==before?after:key.t*(after/before);
    for(auto& cue:c.cues)cue.t=std::clamp(cue.t,0.,after);
}
}
size_t InsertFrame(Clip& c,size_t index){
    const auto frame=c.frames.at(index);const double before=c.Duration();
    c.frames.insert(c.frames.begin()+index+1,frame);RepairLength(c,before);return index+1;
}
bool DeleteFrame(Clip& c,size_t index){
    c.frames.at(index);if(c.frames.size()==1)return false;
    const double before=c.Duration();c.frames.erase(c.frames.begin()+index);RepairLength(c,before);return true;
}
std::string DuplicateClip(Library& lib,const std::string& actor,const std::string& id){
    auto& clips=lib.actors.at(actor);auto copy=clips.at(id);const auto base=id+"_copy";auto name=base;
    for(size_t suffix=2;clips.count(name);++suffix)name=base+std::to_string(suffix);
    copy.id=name;clips.emplace(name,std::move(copy));return name;
}
bool SaveClips(Library& current,std::string& error,const std::string& root){
    const auto target=std::filesystem::path(root)/"data/clips.json";
    const auto temp=std::filesystem::path(root)/"data/clips.json.tmp";
    bool created=false;
    try{
        Json j;{std::ifstream input(target);Require(bool(input),"Cannot open data/clips.json");input>>j;}
        j["actors"]=Json::object();
        for(const auto& actor:current.actors)for(const auto& entry:actor.second){
            const auto& c=entry.second;Require(c.actor==actor.first&&c.id==entry.first,"Clip identity mismatch");
            Json v={{"loop",c.loop},{"composed",c.composed},{"frames",Json::array()},{"tracks",Json::object()},{"cues",Json::array()}};
            for(const auto& f:c.frames){Require(std::isfinite(f.duration)&&f.duration>=20,"Edited frame duration must be at least 20 ms");v["frames"].push_back({{"file",f.file},{"duration",f.duration}});}
            for(const auto& tr:c.tracks){v["tracks"][tr.first]=Json::array();for(const auto& k:tr.second){Require(std::isfinite(k.t)&&std::isfinite(k.value),"Invalid track number");v["tracks"][tr.first].push_back({{"t",k.t},{"value",k.value},{"ease",k.ease}});}}
            for(const auto& q:c.cues){Require(std::isfinite(q.t)&&std::isfinite(q.duration)&&std::isfinite(q.value),"Invalid cue number");v["cues"].push_back({{"t",q.t},{"duration",q.duration},{"value",q.value},{"action",q.action},{"text",q.text}});}
            j["actors"][actor.first][entry.first]=std::move(v);
        }
        auto validated=ParseLibrary(j,root); // Validate everything before touching the destination.
        Require(!std::filesystem::exists(temp),"Save temporary file already exists");
        {std::ofstream output(temp,std::ios::binary);created=true;Require(bool(output),"Cannot create save temporary file");output<<j.dump(2)<<'\n';output.flush();Require(bool(output),"Cannot write save temporary file");output.close();Require(bool(output),"Cannot close save temporary file");}
#ifdef _WIN32
        Require(MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"Cannot replace clips.json");
#else
        Require(std::rename(temp.c_str(),target.c_str())==0,"Cannot replace clips.json");
#endif
        current.actors.swap(validated.actors);error.clear();return true;
    }catch(const std::exception& e){if(created){std::error_code ignored;std::filesystem::remove(temp,ignored);}error=e.what();return false;}
}
bool Reload(Library& current,std::string& error,const std::string& root){
    try{auto next=LoadLibrary(root);current=std::move(next);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}
}
namespace {
std::string DocumentKey(const Library& lib,bool cinematic){
    Json j=Json::array();
    if(cinematic){j.push_back(lib.cinematic.skippable);for(const auto& s:lib.cinematic.steps)
        j.push_back({s.t,int(s.cue),s.value,s.duration,s.enemy,s.pose,s.falling,s.from_x,s.from_y,s.to_x,s.to_y});
    }else for(const auto& a:lib.actors){j.push_back(a.first);for(const auto& entry:a.second){const auto& c=entry.second;
        j.push_back({entry.first,c.actor,c.id,c.loop,c.composed});
        for(const auto& f:c.frames)j.push_back({f.file,f.duration});
        j.push_back("tracks");for(const auto& tr:c.tracks){j.push_back(tr.first);for(const auto& k:tr.second)j.push_back({k.t,k.value,k.ease});}
        j.push_back("cues");for(const auto& q:c.cues)j.push_back({q.t,q.duration,q.value,q.action,q.text});
    }}return j.dump();
}
void RestoreDocument(Library& target,const Library& source,bool cinematic){
    if(cinematic)target.cinematic=source.cinematic;else target.actors=source.actors;
}
}
bool SameDocument(const Library& a,const Library& b,bool cinematic){return DocumentKey(a,cinematic)==DocumentKey(b,cinematic);}
void EditHistory::Record(const Library& before,const Library& after,bool cinematic){
    if(SameDocument(before,after,cinematic))return;
    undo_.push_back({before,after,cinematic});if(undo_.size()>Limit)undo_.erase(undo_.begin());redo_.clear();
}
bool EditHistory::Undo(Library& lib){
    if(undo_.empty())return false;
    auto entry=std::move(undo_.back());undo_.pop_back();RestoreDocument(lib,entry.before,entry.cinematic);redo_.push_back(std::move(entry));return true;
}
bool EditHistory::Redo(Library& lib){
    if(redo_.empty())return false;
    auto entry=std::move(redo_.back());redo_.pop_back();RestoreDocument(lib,entry.after,entry.cinematic);undo_.push_back(std::move(entry));return true;
}
double ImpactTime(const Clip& c){for(const auto& q:c.cues)if(q.action=="impact")return q.t;return c.Duration();}
double ImpactTime(const undergod::Timeline& t){for(const auto& s:t.steps)if(s.cue==undergod::Cue::Impact)return s.t;return CinematicDuration(t);}
Reaction TargetReaction(const Library& lib,const std::string& target,double time,double impact,bool defeat){
    const auto& clips=lib.actors.at(target);double age=time-impact;
    if(age<0||!clips.count("hit"))return {"idle",std::max(0.,time)};
    const double hit=clips.at("hit").Duration();
    if(age<hit)return {"hit",age};
    if(defeat&&clips.count("defeat")&&clips.at("defeat").composed)return {"defeat",age-hit};
    return {"idle",age-hit};
}
double ExchangeDuration(const Library& lib,const std::string& primary,const std::string& target,const std::string& clip){
    const auto& c=lib.actors.at(primary).at(clip);const auto& t=lib.actors.at(target);
    return std::max(c.Duration(),ImpactTime(c)+(t.count("hit")?t.at("hit").Duration():0));
}
double NextSpeed(double speed){return std::clamp((std::floor(speed*4)+1)/4,.25,3.);}
bool AdvanceTransport(double& time,double delta,double duration,bool loop){
    Require(std::isfinite(delta)&&delta>=0&&duration>0,"Invalid transport advance");
    time+=delta;if(!loop&&time>=duration){time=duration;return false;}return true;
}
FrameNeighbors NeighborFrames(const Clip& c,size_t current,bool loop){
    Require(current<c.frames.size(),"Invalid current frame");const size_t last=c.frames.size()-1;
    return {current>0?current-1:(loop?last:0),current<last?current+1:(loop?0:last)};
}
Pose SamplePose(const Clip& c,double ms,bool loop){
    Require(std::isfinite(ms),"Invalid sample time");const double duration=c.Duration();Require(duration>0,"Empty clip");
    double t=std::max(0.,ms);t=loop?std::fmod(t,duration):std::min(t,duration);
    Pose p;double boundary=0;p.frame=c.frames.size()-1;
    for(size_t i=0;i<c.frames.size();++i){boundary+=c.frames[i].duration;if(t<boundary){p.frame=i;break;}}
    std::map<std::string,double*> out={{"x",&p.x},{"y",&p.y},{"scale",&p.scale},{"rotation",&p.rotation},{"r",&p.r},{"g",&p.g},{"b",&p.b},{"alpha",&p.alpha},{"desaturate",&p.desaturate}};
    for(const auto& tr:c.tracks)*out.at(tr.first)=Track(tr.second,t);
    return p;
}
Sample SampleInterval(const Clip& c,double previous,double ms,bool loop){
    Require(std::isfinite(previous)&&std::isfinite(ms)&&ms>=0,"Invalid interval");
    Sample s;s.pose=SamplePose(c,ms,loop);if(ms<previous)return s;
    const double d=c.Duration();const long long first=loop?static_cast<long long>(std::max(0.,std::floor(previous/d))):0;
    const long long last=loop?static_cast<long long>(std::floor(ms/d)):0;
    for(long long cycle=first;cycle<=last;++cycle)for(size_t i=0;i<c.cues.size();++i){double at=cycle*d+c.cues[i].t;if(at>previous&&at<=ms)s.fired.push_back({i,cycle,at});}
    return s;
}
Sample Player::Advance(const Clip& c,double ms,bool loop){auto s=SampleInterval(c,previous_,ms,loop);previous_=ms;return s;}
double CinematicDuration(const undergod::Timeline& t){double end=0;for(const auto& s:t.steps)end=std::max(end,double(s.t));return end;}
CinematicPose SampleCinematic(const Library& lib,double ms,const std::string& primary,const std::string& target){
    using undergod::Cue;CinematicPose p;double playerStart=0,enemyStart=0;
    double zoomStart=1,zoomTarget=1,zoomTime=0,zoomDuration=0;
    double spearTime=0,spearDuration=1,fx=0,fy=0,tx=0,ty=0;
    auto zoom=[&](double t){double u=zoomDuration>0?std::clamp((t-zoomTime)/zoomDuration,0.,1.):1;return zoomStart+(zoomTarget-zoomStart)*(1-(1-u)*(1-u));};
    // Replaying the game's portable cursor makes seeks deterministic, including
    // overlapping zooms sampled at each step boundary rather than at update end.
    undergod::TimelinePlayer player;
    player.Start(lib.cinematic,[&](const undergod::Step& s){
        double late=std::max(0.,ms-s.t);
        switch(s.cue){
        case Cue::Dim:p.dim=s.value;break;
        case Cue::Zoom:zoomStart=zoom(s.t);zoomTarget=s.value;zoomTime=s.t;zoomDuration=s.duration;break;
        case Cue::Pose:if(s.enemy){p.enemyClip=s.pose;enemyStart=s.t;}else{p.playerClip=s.pose;playerStart=s.t;}break;
        case Cue::Vfx:p.vfxAge=late;p.vfxFrames=int(s.value);p.vfxEnemy=s.enemy;break;
        case Cue::Spear:p.spearVisible=true;p.falling=s.falling;spearTime=s.t;spearDuration=s.duration;fx=s.from_x;fy=s.from_y;tx=s.to_x;ty=s.to_y;break;
        case Cue::Shake:p.shake=late<s.duration?std::sin(ms*.095)*s.value*std::min(1.,(s.duration-late)/180.):0;break;
        case Cue::Flash:p.flash=s.duration>0?std::max(0.,1-late/s.duration):0;break;
        case Cue::Impact:case Cue::End:break;
        }
    },[&]{p.impacted=true;},[&]{p.ended=true;p.dim=0;p.spearVisible=false;p.vfxAge=-1;p.shake=p.flash=0;zoomStart=zoomTarget=1;p.playerClip=p.enemyClip="idle";playerStart=enemyStart=ms;});
    player.Advance(float(std::max(0.,ms)));p.zoom=zoom(ms);
    double u=std::clamp((ms-spearTime)/spearDuration,0.,1.);u=p.falling?u*u:1-(1-u)*(1-u);p.spearX=fx+(tx-fx)*u;p.spearY=fy+(ty-fy)*u;
    if(!lib.actors.at(primary).count(p.playerClip))p.playerClip="idle";
    if(!lib.actors.at(target).count(p.enemyClip))p.enemyClip="idle";
    const auto& pc=lib.actors.at(primary).at(p.playerClip);const auto& ec=lib.actors.at(target).at(p.enemyClip);
    p.player=SamplePose(pc,ms-playerStart,pc.loop);p.enemy=SamplePose(ec,ms-enemyStart,ec.loop);
    return p;
}
}
