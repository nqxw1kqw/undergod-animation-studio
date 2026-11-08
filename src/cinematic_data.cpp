// LoadTimeline adapted from Undergod Card src/quest_data.cpp (read-only source).
#include "quest_timeline.hpp"
#include "../third_party/nlohmann/json.hpp"
#include <fstream>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace undergod {
namespace { using Json=nlohmann::json;
Json Read(const std::string& path){std::ifstream f(path);if(!f)throw std::runtime_error("Cannot open "+path);Json j;f>>j;return j;}
}
Timeline LoadTimeline(const std::string& file){return ParseTimelineText(Read(file).dump());}
Timeline ParseTimelineText(const std::string& text){
    const auto j=Json::parse(text);Timeline timeline;timeline.skippable=j.value("skippable",true);int impacts=0,ends=0,falls=0;float impact_time=0,end_time=0,fall_end=0;
    for(const auto& s:j.at("steps")){
        Step step;step.t=s.at("t");const std::string action=s.at("action");
        if(action=="dimScreen"){step.cue=Cue::Dim;step.value=s.at("alpha");}
        else if(action=="cameraZoom"){step.cue=Cue::Zoom;step.value=s.at("to");step.duration=s.at("duration");}
        else if(action=="playSprite"){
            step.cue=Cue::Pose;const std::string target=s.at("target");if(target!="player"&&target!="enemy")throw std::runtime_error("Unknown cinematic actor");step.enemy=target=="enemy";step.pose=s.at("anim");
            if(step.pose!="idle"&&step.pose!="attack"&&step.pose!="hit"&&step.pose!="defend"&&step.pose!="ult_charge"&&step.pose!="ult_throw")throw std::runtime_error("Unknown cinematic pose");
        }
        else if(action=="spawnVFX"){
            step.cue=Cue::Vfx;step.value=s.at("frames");const std::string target=s.at("at");if(target!="player"&&target!="enemy")throw std::runtime_error("Unknown VFX target");step.enemy=target=="enemy";
            if(s.at("sheet")!="vfx/spear_impact.png"||step.value<1||step.value>4)throw std::runtime_error("Unsupported VFX sheet or frame count");
        }
        else if(action=="moveSpear"){
            step.cue=Cue::Spear;const std::string phase=s.at("phase");if(phase!="rise"&&phase!="fall")throw std::runtime_error("Invalid spear phase");step.falling=phase=="fall";
            step.duration=s.at("duration");step.from_x=s.at("from").at(0);step.from_y=s.at("from").at(1);step.to_x=s.at("to").at(0);step.to_y=s.at("to").at(1);
            if(step.duration<=0)throw std::runtime_error("Spear movement needs a positive duration");
            if(step.falling){++falls;fall_end=step.t+step.duration;}
        }
        else if(action=="shake"){step.cue=Cue::Shake;step.value=s.at("intensity");step.duration=s.at("duration");}
        else if(action=="impact"){if(s.at("callback")!="applyDamage")throw std::runtime_error("Unknown cinematic callback");step.cue=Cue::Impact;++impacts;impact_time=step.t;}
        else if(action=="flashWhite"){step.cue=Cue::Flash;step.duration=s.at("duration");}
        else if(action=="end"){step.cue=Cue::End;++ends;end_time=step.t;}
        else throw std::runtime_error("Unknown cinematic action: "+action);
        if(!std::isfinite(step.t)||!std::isfinite(step.duration)||!std::isfinite(step.value)||!std::isfinite(step.from_x)||!std::isfinite(step.from_y)||!std::isfinite(step.to_x)||!std::isfinite(step.to_y)||step.t<0||step.duration<0)throw std::runtime_error("Invalid cinematic timing");
        timeline.steps.push_back(step);
    }
    if(impacts!=1||ends!=1||impact_time>end_time)throw std::runtime_error("Cinematic needs one impact before end");
    if(falls!=1||std::abs(fall_end-impact_time)>.1f)throw std::runtime_error("Spear contact must match damage timing");
    std::stable_sort(timeline.steps.begin(),timeline.steps.end(),[](const auto& a,const auto& b){return a.t<b.t;});
    if(timeline.steps.empty()||timeline.steps.back().cue!=Cue::End)throw std::runtime_error("End must be the last cinematic step");
    for(const auto& s:timeline.steps){
        if((s.cue==Cue::Zoom||s.cue==Cue::Shake||s.cue==Cue::Flash)&&s.duration<=0)throw std::runtime_error("Effect duration must be positive");
        if((s.cue==Cue::Dim&&(s.value<0||s.value>1))||(s.cue==Cue::Zoom&&s.value<=0)||(s.cue==Cue::Shake&&s.value<0))throw std::runtime_error("Effect value out of range");
        if(s.cue!=Cue::End&&s.t+s.duration>end_time)throw std::runtime_error("Cinematic effect exceeds end");
    }
    return timeline;
}
std::string SerializeTimeline(const Timeline& timeline){
    Json j={{"id","sao_roi"},{"skippable",timeline.skippable},{"steps",Json::array()}};
    for(const auto& s:timeline.steps){
        Json v={{"t",s.t}};
        switch(s.cue){
        case Cue::Dim:v["action"]="dimScreen";v["alpha"]=s.value;break;
        case Cue::Zoom:v["action"]="cameraZoom";v["to"]=s.value;v["duration"]=s.duration;break;
        case Cue::Pose:v["action"]="playSprite";v["target"]=s.enemy?"enemy":"player";v["anim"]=s.pose;break;
        case Cue::Vfx:v["action"]="spawnVFX";v["sheet"]="vfx/spear_impact.png";v["at"]=s.enemy?"enemy":"player";v["frames"]=s.value;break;
        case Cue::Spear:v["action"]="moveSpear";v["phase"]=s.falling?"fall":"rise";v["from"]={s.from_x,s.from_y};v["to"]={s.to_x,s.to_y};v["duration"]=s.duration;break;
        case Cue::Shake:v["action"]="shake";v["intensity"]=s.value;v["duration"]=s.duration;break;
        case Cue::Impact:v["action"]="impact";v["callback"]="applyDamage";break;
        case Cue::Flash:v["action"]="flashWhite";v["duration"]=s.duration;break;
        case Cue::End:v["action"]="end";break;
        default:throw std::runtime_error("Unknown cinematic cue");
        }
        // JSON would silently turn NaN into null, so reject all fields first.
        for(float n:{s.t,s.value,s.duration,s.from_x,s.from_y,s.to_x,s.to_y})if(!std::isfinite(n))throw std::runtime_error("Invalid cinematic number");
        j["steps"].push_back(v);
    }
    return j.dump(2)+"\n";
}

}
