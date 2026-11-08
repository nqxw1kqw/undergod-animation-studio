#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <DxLib.h>
#include "animator.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace {
using namespace studio;
constexpr int W=1440,H=1160;
struct Rect { int x,y,w,h;bool Has(int mx,int my)const{return mx>=x&&mx<x+w&&my>=y&&my<y+h;} };
unsigned C(int r,int g,int b){return GetColor(r,g,b);}
void Blend(double alpha=1){SetDrawBlendMode(alpha>=1?DX_BLENDMODE_NOBLEND:DX_BLENDMODE_ALPHA,int(std::clamp(alpha,0.,1.)*255));}
std::string Fixed(double n,int digits=2){std::ostringstream s;s<<std::fixed<<std::setprecision(digits)<<n;return s.str();}
std::string CueName(undergod::Cue c){switch(c){
 case undergod::Cue::Dim:return "dimScreen";case undergod::Cue::Zoom:return "cameraZoom";
 case undergod::Cue::Pose:return "playSprite";case undergod::Cue::Vfx:return "spawnVFX";
 case undergod::Cue::Spear:return "moveSpear";case undergod::Cue::Shake:return "shake";
 case undergod::Cue::Impact:return "impact";case undergod::Cue::Flash:return "flashWhite";
 case undergod::Cue::End:return "end";}return "?";}
struct Images {
 std::map<std::string,std::pair<int,int>> handles;
 Images()=default;Images(const Images&)=delete;Images& operator=(const Images&)=delete;
 ~Images(){Clear();}
 void Clear(){for(const auto& h:handles){DeleteGraph(h.second.first);if(h.second.second>=0)DeleteGraph(h.second.second);}handles.clear();}
 void Load(const Library& lib){
  auto add=[&](const std::string& file,bool grayscale){if(handles.count(file))return;
   const int h=LoadGraph(file.c_str());if(h<0)throw std::runtime_error("Cannot decode image: "+file);
   handles[file]={h,-1};int w=0,height=0;GetGraphSize(h,&w,&height);if(w!=512||height!=512)throw std::runtime_error("Expected 512x512 image: "+file);
   if(grayscale){const int gray=LoadGraph(file.c_str());handles[file].second=gray;if(gray<0||GraphFilter(gray,DX_GRAPH_FILTER_MONO,0,0)<0)throw std::runtime_error("Cannot create grayscale image: "+file);}
  };
  for(const auto& actor:lib.actors)for(const auto& clip:actor.second)for(const auto& frame:clip.second.frames)add(frame.file,true);
  add("assets/pastel/vfx/spear.png",false);add("assets/pastel/vfx/spear_impact.png",false);
 }
 int Get(const std::string& file,bool gray=false)const{const auto& pair=handles.at(file);return gray?pair.second:pair.first;}
};
struct Effects {bool shake=true,flash=true,particles=true,dim=true,reduced=false;};
struct Studio {
 Library lib;Images images;int stage=-1,sprite=-1,font=-1;std::string actor="chick",target="frog",clip="idle",notice;
 double time=0,speed=1,noticeLeft=0;size_t selectedRow=0;int backdrop=0;bool guides=false;bool exchange=false;bool playing=true,loop=true,onion=false;Effects effects;
 ~Studio(){if(sprite>=0)DeleteGraph(sprite);if(stage>=0)DeleteGraph(stage);if(font>=0)DeleteFontToHandle(font);}
 bool Cinematic()const{return clip=="sao_roi";}
 const Clip& Current()const{return lib.actors.at(actor).at(clip);}
 double Duration()const{return Cinematic()?CinematicDuration(lib.cinematic):Current().Duration();}
 double LocalTime()const{if(exchange)return time;return loop?std::fmod(time,Duration()):std::min(time,Duration());}
 void Select(std::string a,std::string c){exchange=false;actor=std::move(a);clip=std::move(c);if(!Cinematic()&&!lib.actors.at(actor).count(clip))clip="idle";selectedRow=0;time=0;loop=Cinematic()?false:Current().loop;playing=true;}
 void Reload(){
  try{auto next=LoadLibrary();Images nextImages;nextImages.Load(next);
   images.handles.swap(nextImages.handles);lib=std::move(next);
   if(!Cinematic()&&!lib.actors.at(actor).count(clip))clip="idle";
   time=0;notice="Reloaded clips + cinematic";noticeLeft=3;
  }catch(const std::exception& e){notice=std::string("Reload failed (previous data kept): ")+e.what();noticeLeft=0;}
 }
 void Init(){
  lib=LoadLibrary();images.Load(lib);stage=MakeScreen(W,360,TRUE);sprite=MakeScreen(512,512,TRUE);
  font=CreateFontToHandle("Segoe UI",19,5,DX_FONTTYPE_ANTIALIASING_4X4);
  if(stage<0||sprite<0||font<0)throw std::runtime_error("Cannot create stage/font");
  SetFontCharCodeFormatToHandle(DX_CHARCODEFORMAT_UTF8,font);SetDrawMode(DX_DRAWMODE_BILINEAR);
 }
 void Text(int x,int y,const std::string& text,unsigned color=0x343948){DrawStringToHandle(x,y,text.c_str(),color,font);}
 void Button(Rect r,const std::string& text,bool on=false){
  DrawRoundRect(r.x,r.y,r.x+r.w,r.y+r.h,7,7,on?C(79,106,91):C(225,229,218),TRUE);
  Text(r.x+10,r.y+7,text,on?C(255,255,246):C(52,57,72));
 }
 void Step(int direction){
  playing=false;double at=LocalTime();
  if(Cinematic()){time=std::clamp((direction>0?std::floor(at/125)+1:std::ceil(at/125)-1)*125,0.,Duration());return;}
  std::vector<double> boundaries{0};double t=0;for(const auto& f:Current().frames){t+=f.duration;boundaries.push_back(t);}
  double target=direction>0?Duration():0;
  if(direction>0){for(double b:boundaries)if(b>at+.0001){target=b;break;}}
  else{for(auto i=boundaries.rbegin();i!=boundaries.rend();++i)if(*i<at-.0001){target=*i;break;}}
  time=target;if(loop&&time==Duration())time=0;
 }
 void Actor(const std::string& who,const std::string& name,Pose p,double home,double opacity=1){
  if(effects.reduced){if(name=="victory")p.y=0;if(name=="ult_charge")p.scale=1;}
  const auto& c=lib.actors.at(who).at(name);const auto& file=c.frames.at(p.frame).file;
  if(opacity==1){Blend(.15);DrawOval(int(home+p.x),324,who=="frog"?70:46,9,C(80,107,76),TRUE);Blend();}
  int graph=images.Get(file);
  if(p.desaturate>=1)graph=images.Get(file,true);
  else if(p.desaturate>0){
   SetDrawScreen(sprite);ClearDrawScreen();Blend();DrawGraph(0,0,graph,TRUE);
   Blend(p.desaturate);DrawGraph(0,0,images.Get(file,true),TRUE);Blend();
   SetDrawScreen(stage);graph=sprite;
  }
  SetDrawBright(int(p.r),int(p.g),int(p.b));
  auto draw=[&](int graph,double alpha){Blend(alpha);DrawRotaGraph3F(float(home+p.x),float(320+p.y),256,456,p.scale*.5,p.scale*.5,p.rotation,graph,TRUE,home>720?TRUE:FALSE);};
  draw(graph,p.alpha*opacity);
  SetDrawBright(255,255,255);Blend();
 }
 void Particles(double x,double age,double life,int count,bool dust){
  if(!effects.particles||effects.reduced)return;
  const double seconds=age/1000.;
  for(int i=0;i<std::min(count,64);++i){
   // Game terminal dust velocities and ballistic particle gravity, deterministic.
   double vx=dust?(i-6)*13:((i*37)%160-80),vy=dust?(-30-(i%3)*15):(-30-(i*29)%90);
   Blend(.7*(1-age/life));DrawCircle(int(x+vx*seconds),int(205+vy*seconds+160*seconds*seconds),3,C(235,185,100),TRUE);
  }Blend();
 }
 struct Active {const Cue* cue;double age;};
 std::vector<Active> ActiveCues()const{
  std::vector<Active> result;if(Cinematic())return result;const auto& c=Current();
  const long long cycle=loop?static_cast<long long>(std::floor(time/c.Duration())):0;
  for(const auto& cue:c.cues){const long long back=loop?static_cast<long long>(std::ceil(cue.duration/c.Duration())):0;
   for(long long k=std::max(0LL,cycle-back);k<=cycle;++k){double age=time-(k*c.Duration()+cue.t);if(age>=0&&age<cue.duration)result.push_back({&cue,age});}
  }return result;
 }
 void World(){
  const double at=LocalTime();CinematicPose cinematic;std::vector<Active> active=ActiveCues();
  double dim=0,zoom=1,shake=0,flash=0;
  if(Cinematic()){cinematic=SampleCinematic(lib,at);dim=cinematic.dim;zoom=cinematic.zoom;shake=cinematic.shake;flash=cinematic.flash;}
  else for(const auto& a:active){const auto& q=*a.cue;
   if(q.action=="dim")dim=q.value;
   if(q.action=="zoom"){double u=std::clamp(a.age/q.duration,0.,1.);zoom=1+(q.value-1)*(1-(1-u)*(1-u));}
   if(q.action=="shake")shake+=std::sin(time*.095)*q.value*std::min(1.,(q.duration-a.age)/180.);
   if(q.action=="flash")flash=std::max(flash,q.value*(1-a.age/q.duration));
  }
  if(!effects.dim)dim=0;
  if(effects.reduced){dim*=70./255;zoom=1;}
  if(!effects.shake||effects.reduced)shake=0;
  if(!effects.flash||effects.reduced)flash=0;
  SetDrawScreen(stage);ClearDrawScreen();
  if(backdrop==0){
  for(int y=0;y<360;++y){double f=y/360.;DrawLine(0,y,W,y,C(int(227+17*f),int(240-3*f),int(237-23*f)));}
  for(int i=0;i<7;++i)DrawOval(i*280,330,240,170,C(196,217,191),TRUE);
  for(int i=0;i<6;++i)DrawOval(i*330-70,360,210,150,C(174,201,164),TRUE);
  DrawBox(0,306,W,360,C(220,227,182),TRUE);DrawBox(0,306,W,310,C(143,169,124),TRUE);
  }else if(backdrop==1)DrawBox(0,0,W,360,C(35,39,48),TRUE);
  else for(int y=0;y<360;y+=24)for(int x=0;x<W;x+=24){int shade=((x/24+y/24)%2)?175:215;DrawBox(x,y,x+24,y+24,C(shade,shade,shade),TRUE);}
  auto reaction=TargetReaction(lib,target,at,Cinematic()?ImpactTime(lib.cinematic):ImpactTime(Current()),Cinematic());
  if(Cinematic()&&cinematic.ended)reaction={"idle",0};
  if(Cinematic()||clip=="attack"||clip=="ult_charge"||clip=="ult_throw")Actor(target,reaction.clip,SamplePose(lib.actors.at(target).at(reaction.clip),reaction.local,lib.actors.at(target).at(reaction.clip).loop),1120);
  else Actor(target,"idle",SamplePose(lib.actors.at(target).at("idle"),time,true),1120);
  Blend(dim);DrawBox(0,0,W,360,C(33,28,60),TRUE);Blend();
  if(Cinematic())Actor(actor,lib.actors.at(actor).count(cinematic.playerClip)?cinematic.playerClip:"idle",SamplePose(lib.actors.at(actor).at(lib.actors.at(actor).count(cinematic.playerClip)?cinematic.playerClip:"idle"),at-250,false),320);
  else{
   auto pose=SamplePose(Current(),time,loop);const double home=320;
   if(onion){const auto neighbors=NeighborFrames(Current(),pose.frame,loop);auto ghost=pose;
    if(neighbors.previous!=pose.frame){ghost.frame=neighbors.previous;Actor(actor,clip,ghost,home,.35);}
    if(neighbors.next!=pose.frame){ghost.frame=neighbors.next;Actor(actor,clip,ghost,home,.20);}
   }
   Actor(actor,clip,pose,home);
  }
  if(Cinematic()){
   if(cinematic.spearVisible)DrawRotaGraph3F(float(cinematic.spearX),float(cinematic.spearY),256,56,.45,.45,cinematic.falling?3.141592653589793:0,images.Get("assets/pastel/vfx/spear.png"),TRUE);
   if(cinematic.vfxAge>=0&&cinematic.vfxAge<cinematic.vfxFrames*125){int f=std::min(cinematic.vfxFrames-1,int(cinematic.vfxAge/125));DrawRectRotaGraph3F(cinematic.vfxEnemy?1120.f:320.f,195,(f%2)*256,(f/2)*256,256,256,128,128,1,1,0,images.Get("assets/pastel/vfx/spear_impact.png"),TRUE);}
   // The game's impact produces damage feedback in addition to the VFX sheet.
   for(const auto& s:lib.cinematic.steps)if(s.cue==undergod::Cue::Impact&&at>=s.t&&at<s.t+400)Particles(1120,at-s.t,400,10,false);
  }else for(const auto& a:active){const auto& q=*a.cue;double x=actor=="chick"?320:1120;
   if(q.action=="particles")Particles(x,a.age,q.duration,int(q.value),clip=="defeat");
   if(q.action=="popup"){Blend(1-a.age/q.duration);Text(int(x-35),int(140-a.age*.07),q.text,C(172,85,106));Blend();}
  }
  Blend(flash);DrawBox(0,0,W,360,C(255,255,255),TRUE);Blend();
  if(guides){
   DrawLine(0,320,W,320,C(230,115,65));
   auto origin=[&](int x){DrawLine(x-14,320,x+14,320,C(255,80,140),2);DrawLine(x,306,x,334,C(255,80,140),2);};
   origin(320);origin(1120);
  }
  SetDrawScreen(DX_SCREEN_BACK);SetDrawArea(20,220,W-20,580);
  DrawRotaGraph3F(float(W/2+shake),400,W/2.f,180,zoom,zoom,0,stage,TRUE);SetDrawArea(0,0,W,H);
 }
 std::vector<std::string> Clips()const{std::vector<std::string> names;for(const auto& c:lib.actors.at(actor))names.push_back(c.first);names.push_back("sao_roi");return names;}
 void Timeline(){
  Text(24,904,"Timeline: Up/Down select | [ / ] cue -/+10 ms (Shift: 50) | , / . frame -/+10 ms | Ctrl+S save");
  if(Cinematic()){Text(24,936,"Cinematic is read-only");return;}
  const auto& c=Current();const size_t count=c.frames.size()+c.cues.size();
  selectedRow=std::min(selectedRow,count-1);
  const size_t first=selectedRow>=6?selectedRow-5:0;
  for(size_t row=first;row<std::min(count,first+7);++row){
   std::string label;
   if(row<c.frames.size()){const auto& f=c.frames[row];label="Frame "+std::to_string(row)+"  "+f.file+"  "+Fixed(f.duration,1)+" ms";}
   else{const auto& q=c.cues[row-c.frames.size()];label="Cue "+Fixed(q.t,1)+" ms  "+q.action;}
   int y=936+int(row-first)*30;
   if(row==selectedRow)DrawBox(20,y-2,W-20,y+26,C(220,229,211),TRUE);
   Text(24,y,(row==selectedRow?"> ":"  ")+label);
  }
 }
 void Draw(){
  ClearDrawScreen();DrawBox(0,0,W,H,C(250,248,239),TRUE);
  Text(24,18,"UNDERGOD / ANIMATION STUDIO v1");Text(750,18,"A primary | T target | X preview exchange | R reload");
  Button({24,58,150,36},"Chick [1]",actor=="chick");Button({184,58,150,36},"Frog [2]",actor=="frog");
  Button({354,58,260,36},"Target [T]: "+target);Button({624,58,230,36},"Preview exchange [X]");
  auto names=Clips();for(size_t i=0;i<names.size();++i)Button({24+int(i%9)*155,110+int(i/9)*42,145,36},names[i],clip==names[i]);
  Text(24,176,Cinematic()?"Sao Roi / shared two-actor cinematic":actor+" / "+clip+(Current().composed?" / composed from existing frames":""));
  World();Timeline();Text(1000,672,onion?"Onion [O]: on":"Onion [O]: off");
  Button({24,612,150,36},playing?"Pause [Space]":"Play [Space]",playing);
  Button({184,612,160,36},"Restart [Home]");Button({354,612,150,36},"Loop [L]",loop);
  Button({514,612,100,36},"- [PgDn]");Button({624,612,100,36},"+ [PgUp]");Text(740,621,Fixed(speed)+"x");
  Button({850,612,145,36},"< frame [Left]");Button({1005,612,155,36},"frame > [Right]");
  const Rect toggles[]={{24,664,160,36},{194,664,160,36},{364,664,180,36},{554,664,150,36},{714,664,245,36}};
  Button(toggles[0],"Shake [S]",effects.shake);Button(toggles[1],"Flash [F]",effects.flash);Button(toggles[2],"Particles [P]",effects.particles);Button(toggles[3],"Dim [D]",effects.dim);Button(toggles[4],"Reduced effects [E]",effects.reduced);
  Text(24,708,std::string("Backdrop [B]: ")+(backdrop==0?"arena":backdrop==1?"dark":"checkerboard")+" | Guides [G]: "+(guides?"on":"off"));
  const double u=std::clamp(LocalTime()/Duration(),0.,1.);DrawBox(48,740,W-48,750,C(207,217,200),TRUE);DrawBox(48,740,48+int((W-96)*u),750,C(79,106,91),TRUE);DrawCircle(48+int((W-96)*u),745,8,C(79,106,91),TRUE);
  size_t frame=Cinematic()?SampleCinematic(lib,LocalTime()).player.frame:SamplePose(Current(),time,loop).frame;
  const size_t lastFrame=Cinematic()?lib.actors.at("chick").at(SampleCinematic(lib,LocalTime()).playerClip).frames.size()-1:Current().frames.size()-1;
  Text(24,774,"Time "+Fixed(LocalTime()/1000,3)+" / "+Fixed(Duration()/1000,3)+" s   |   frame "+std::to_string(frame)+" / "+std::to_string(lastFrame)+"   |   "+(playing?"playing":"paused"));
  std::string cueText;
  if(Cinematic()){
   const double at=LocalTime();
   for(size_t i=0;i<lib.cinematic.steps.size();++i){const auto& s=lib.cinematic.steps[i];bool on=false;
    switch(s.cue){
     case undergod::Cue::Dim:case undergod::Cue::Pose:case undergod::Cue::Spear:{
      double until=Duration();
      for(size_t k=i+1;k<lib.cinematic.steps.size();++k){const auto& next=lib.cinematic.steps[k];
       if(next.cue==s.cue&&(s.cue!=undergod::Cue::Pose||next.enemy==s.enemy)){until=next.t;break;}
      }
      on=at>=s.t&&at<until;break;
     }
     case undergod::Cue::Vfx:on=at>=s.t&&at<s.t+s.value*125;break;
     default:on=at>=s.t&&at<s.t+std::max(1.f,s.duration);break;
    }
    if(on)cueText+=CueName(s.cue)+" ";
   }
  }else for(const auto& a:ActiveCues())cueText+=a.cue->action+" ";
  Text(24,803,"Active cues: "+(cueText.empty()?"none":cueText));
  Text(24,832,"Drag timeline to scrub | Shift+Left/Right: 50 ms | End: last pose | K: skip cinematic | Esc: exit");
  if(!notice.empty())Text(24,864,notice.substr(0,145),notice.find("failed")!=std::string::npos?C(180,65,70):C(70,120,80));
 }
 void Input(const char* keys,const char* previous,int mx,int my,bool clicked,bool held){
  auto press=[&](int k){return keys[k]&&!previous[k];};
  auto button=[&](Rect r){return clicked&&r.Has(mx,my);};
  if(press(KEY_INPUT_1)||button({24,58,150,36}))Select("chick",clip);
  if(press(KEY_INPUT_2)||button({184,58,150,36}))Select("frog",clip);
  if(press(KEY_INPUT_A)){
   auto next=lib.actors.find(actor);if(++next==lib.actors.end())next=lib.actors.begin();Select(next->first,clip);
  }
  if(press(KEY_INPUT_T)||button({354,58,260,36})){auto next=lib.actors.find(target);if(++next==lib.actors.end())next=lib.actors.begin();target=next->first;time=0;}
  if(press(KEY_INPUT_X)||button({624,58,230,36})){Select(actor,"attack");exchange=true;loop=false;}
  const auto names=Clips();
  if(press(KEY_INPUT_TAB)){auto i=std::find(names.begin(),names.end(),clip)-names.begin();Select(actor,names[(i+1)%names.size()]);}
  for(size_t i=0;i<names.size();++i)if(button({24+int(i%9)*155,110+int(i/9)*42,145,36}))Select(actor,names[i]);
  if(press(KEY_INPUT_SPACE)||button({24,612,150,36})){if(!playing&&!loop&&time>=Duration())time=0;playing=!playing;}
  if(press(KEY_INPUT_HOME)||button({184,612,160,36}))time=0;
  if(press(KEY_INPUT_L)||button({354,612,150,36})){time=LocalTime();loop=!loop;}
  if(press(KEY_INPUT_PGDN)||button({514,612,100,36}))speed=std::max(.25,speed-.25);
  if(press(KEY_INPUT_PGUP)||button({624,612,100,36}))speed=std::min(3.,speed+.25);
  bool shift=keys[KEY_INPUT_LSHIFT]||keys[KEY_INPUT_RSHIFT];
  if(press(KEY_INPUT_LEFT)||button({850,612,145,36})){if(shift){playing=false;time=std::max(0.,LocalTime()-50);}else Step(-1);}
  if(press(KEY_INPUT_RIGHT)||button({1005,612,155,36})){if(shift){playing=false;time=std::min(Duration(),LocalTime()+50);}else Step(1);}
  if(press(KEY_INPUT_END)){playing=false;time=Duration();if(loop)time=std::max(0.,time-.001);}
  if(press(KEY_INPUT_K)&&Cinematic()&&lib.cinematic.skippable){playing=false;time=Duration();loop=false;}
  bool ctrl=keys[KEY_INPUT_LCONTROL]||keys[KEY_INPUT_RCONTROL];
  if(ctrl&&press(KEY_INPUT_S)&&!Cinematic()){std::string error;bool ok=SaveClips(lib,error);notice=ok?"Saved data/clips.json":"Save failed: "+error;noticeLeft=ok?3:0;}
  if(!Cinematic()){
   auto& c=lib.actors.at(actor).at(clip);size_t count=c.frames.size()+c.cues.size();selectedRow=std::min(selectedRow,count-1);
   if(press(KEY_INPUT_UP))selectedRow=(selectedRow+count-1)%count;
   if(press(KEY_INPUT_DOWN))selectedRow=(selectedRow+1)%count;
   if(selectedRow>=c.frames.size()&&(press(KEY_INPUT_LBRACKET)||press(KEY_INPUT_RBRACKET))){playing=false;selectedRow=c.frames.size()+NudgeCue(c,selectedRow-c.frames.size(),(press(KEY_INPUT_LBRACKET)?-1:1)*(shift?50.:10.));}
   if(selectedRow<c.frames.size()&&(press(KEY_INPUT_COMMA)||press(KEY_INPUT_PERIOD))){playing=false;EditFrameDuration(c,selectedRow,press(KEY_INPUT_COMMA)?-10:10);time=std::min(time,Duration());}
  }
  if((!ctrl&&press(KEY_INPUT_S))||button({24,664,160,36}))effects.shake=!effects.shake;
  if(press(KEY_INPUT_F)||button({194,664,160,36}))effects.flash=!effects.flash;
  if(press(KEY_INPUT_P)||button({364,664,180,36}))effects.particles=!effects.particles;
  if(press(KEY_INPUT_D)||button({554,664,150,36}))effects.dim=!effects.dim;
  if(press(KEY_INPUT_E)||button({714,664,245,36}))effects.reduced=!effects.reduced;
  if(held&&Rect{48,725,W-96,40}.Has(mx,my)){playing=false;time=Duration()*std::clamp(double(mx-48)/(W-96),0.,1.);if(loop&&time==Duration())time-=.001;}
  if(press(KEY_INPUT_B))backdrop=(backdrop+1)%3;
  if(press(KEY_INPUT_G))guides=!guides;
  if(press(KEY_INPUT_O))onion=!onion;
  if(press(KEY_INPUT_R))Reload();
 }
 int Smoke(){
  std::filesystem::create_directories("artifacts");std::ofstream report("artifacts/smoke.txt");bool ok=true;
  auto capture=[&](const std::string& file){Draw();int result=SaveDrawScreenToPNG(0,0,W,H,("artifacts/"+file+".png").c_str());ok&=result==0;ScreenFlip();ProcessMessage();};
  report<<"RESULT decoded_images "<<(images.handles.size()==46?"PASS":"FAIL")<<" ("<<images.handles.size()<<"/46)\n";ok&=images.handles.size()==46;
  for(const auto& a:lib.actors)for(const auto& c:a.second){Select(a.first,c.first);loop=false;time=c.second.Duration()/2;capture(a.first+"-"+c.first);}
  Select("chick","sao_roi");for(double t:{0.,1100.,2025.,2700.,3275.,3600.}){time=t;capture("cinematic-"+std::to_string(int(t)));}
  effects.reduced=true;time=2700;capture("reduced-effects");
  report<<"RESULT studio_render_smoke "<<(ok?"PASS":"FAIL")<<'\n';return ok?0:3;
 }
};
}
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR args,int){
 wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::filesystem::current_path(std::filesystem::path(exe).parent_path());
 ChangeWindowMode(TRUE);SetGraphMode(W,H,32);SetWindowSize(1152,720);SetMainWindowText("Undergod Animation Studio v1");
 SetAlwaysRunFlag(TRUE);SetOutApplicationLogValidFlag(FALSE);SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
 if(DxLib_Init()<0)return 1;
 SetDrawScreen(DX_SCREEN_BACK);int result=0;
 {Studio app;try{
  app.Init();if(std::string(args?args:"").find("--smoke")!=std::string::npos)result=app.Smoke();
  else{char previous[256]{};int oldMouse=0;auto last=GetNowHiPerformanceCount();
   while(ProcessMessage()==0){const auto start=GetNowHiPerformanceCount();double dt=std::min(.1,double(start-last)/1000000.);last=start;
    char keys[256]{};GetHitKeyStateAll(keys);if(keys[KEY_INPUT_ESCAPE])break;
    POINT mouse{};RECT client{};GetCursorPos(&mouse);ScreenToClient(GetMainWindowHandle(),&mouse);GetClientRect(GetMainWindowHandle(),&client);
    int mx=int(static_cast<long long>(mouse.x)*W/std::max(1L,client.right)),my=int(static_cast<long long>(mouse.y)*H/std::max(1L,client.bottom));
    int buttons=GetMouseInput();app.Input(keys,previous,mx,my,(buttons&MOUSE_INPUT_LEFT)&&!(oldMouse&MOUSE_INPUT_LEFT),(buttons&MOUSE_INPUT_LEFT)!=0);oldMouse=buttons;
    std::copy(keys,keys+256,previous);
    if(app.playing){app.time+=dt*1000*app.speed;if(app.exchange&&app.time>=ExchangeDuration(app.lib,app.actor,app.target,app.clip)){app.Select(app.actor,"idle");}
    else if(!app.loop&&app.time>=app.Duration()&&!app.exchange){app.time=app.Duration();app.playing=false;}}
    if(app.noticeLeft>0){app.noticeLeft-=dt;if(app.noticeLeft<=0)app.notice.clear();}
    app.Draw();ScreenFlip();int wait=16-int((GetNowHiPerformanceCount()-start)/1000);if(wait>0)WaitTimer(wait);
   }
  }
 }catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Undergod Animation Studio",MB_OK|MB_ICONERROR);result=2;}}
 DxLib_End();return result;
}
