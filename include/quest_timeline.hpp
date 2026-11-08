#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
namespace undergod {
enum class Cue { Dim,Zoom,Pose,Vfx,Spear,Shake,Impact,Flash,End };
struct Step { float t=0; Cue cue=Cue::End; float value=0,duration=0; bool enemy=false; std::string pose="ult_charge"; bool falling=false; float from_x=0,from_y=0,to_x=0,to_y=0; };
struct Timeline { std::vector<Step> steps; bool skippable=true; };
Timeline LoadTimeline(const std::string& file="data/cinematic.json");
class TimelinePlayer {
    Timeline timeline_;size_t cursor_=0;bool done_=true,impacted_=false;
    std::function<void(const Step&)> cue_;
    std::function<void()> impact_,end_;
    void Impact(){if(!impacted_){impacted_=true;impact_();}}
public:
    void Start(Timeline timeline,std::function<void(const Step&)> cue,std::function<void()> impact,std::function<void()> end){
        timeline_=std::move(timeline);std::stable_sort(timeline_.steps.begin(),timeline_.steps.end(),[](const auto& a,const auto& b){return a.t<b.t;});
        cursor_=0;done_=false;impacted_=false;cue_=std::move(cue);impact_=std::move(impact);end_=std::move(end);Advance(0);
    }
    void Advance(float ms){while(!done_ && cursor_<timeline_.steps.size() && timeline_.steps[cursor_].t<=ms){const auto step=timeline_.steps[cursor_++];if(step.cue==Cue::Impact)Impact();else if(step.cue==Cue::End)Finish();else cue_(step);}}
    void Finish(){if(done_)return;done_=true;Impact();end_();}
    void Skip(){if(timeline_.skippable)Finish();}
    bool Active()const{return !done_;}
    void Cancel(){done_=true;cue_={};impact_={};end_={};}
};
}
