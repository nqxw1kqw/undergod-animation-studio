#pragma once
#include "quest_timeline.hpp"
#include <map>
#include <string>
#include <vector>

namespace studio {
// All timeline values use milliseconds; poses use game stage pixels and radians.
struct Key { double t=0,value=0; std::string ease="linear"; };
struct Cue { double t=0,duration=0,value=0; std::string action,text; };
struct Frame { std::string file; double duration=0; };
struct Clip {
    std::string actor,id; bool loop=false,composed=false;
    std::vector<Frame> frames; std::map<std::string,std::vector<Key>> tracks;
    std::vector<Cue> cues;
    double Duration() const;
};
struct Pose {
    size_t frame=0; double x=0,y=0,scale=1,rotation=0,r=255,g=255,b=255,alpha=1,desaturate=0;
};
struct Fired { size_t index=0; long long cycle=0; double at=0; };
struct Sample { Pose pose; std::vector<Fired> fired; };
struct Library {
    std::map<std::string,std::map<std::string,Clip>> actors;
    undergod::Timeline cinematic;
};
Library LoadLibrary(const std::string& root=".");
// Transactional: assignment happens only after both files and references validate.
bool Reload(Library& current,std::string& error,const std::string& root=".");
size_t NudgeStep(undergod::Timeline&,size_t,double);
bool SaveCinematic(Library&,std::string& error,const std::string& root=".");
size_t NudgeCue(Clip&,size_t index,double delta);
void EditFrameDuration(Clip&,size_t index,double delta);
bool SaveClips(Library&,std::string& error,const std::string& root=".");
// Missing impact cues default to the attacking clip's end.
double ImpactTime(const Clip&);
double ImpactTime(const undergod::Timeline&);
struct Reaction {std::string clip="idle";double local=0;};
Reaction TargetReaction(const Library&,const std::string& target,double time,double impact,bool defeat=false);
double ExchangeDuration(const Library&,const std::string& primary,const std::string& target,const std::string& clip);
struct FrameNeighbors {size_t previous=0,next=0;};
FrameNeighbors NeighborFrames(const Clip&,size_t current,bool loop);
Pose SamplePose(const Clip&,double ms,bool loop);
Sample SampleInterval(const Clip&,double previous,double ms,bool loop);
// A monotonic cursor makes repeated Advance calls idempotent. Seek deliberately
// resets delivery history; visual sampling itself is independent of this cursor.
class Player {
    double previous_=-1;
public:
    void Restart(){previous_=-1;}
    void Seek(double ms){previous_=ms;}
    Sample Advance(const Clip& c,double ms,bool loop);
};
struct CinematicPose {
    Pose player,enemy; std::string playerClip="idle",enemyClip="idle";
    double dim=0,zoom=1,shake=0,flash=0,vfxAge=-1;
    int vfxFrames=4; bool vfxEnemy=true,spearVisible=false,falling=false,ended=false,impacted=false;
    double spearX=0,spearY=0;
};
CinematicPose SampleCinematic(const Library&,double ms);
double CinematicDuration(const undergod::Timeline&);
}
