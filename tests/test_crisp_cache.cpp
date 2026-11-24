
#include <SDL.h>
static int failNthUpload=0,totalUploads=0;
static int UploadForTest(SDL_Texture*t,const SDL_Rect*r,const void*p,int pitch){++totalUploads;if(failNthUpload>0&&--failNthUpload==0)return -1;return SDL_UpdateTexture(t,r,p,pitch);}
#define SDL_UpdateTexture UploadForTest
#include "gpucompose.h"
#undef SDL_UpdateTexture
#include <iostream>
#include <cassert>
#include <functional>
#include <string>
static std::vector<uint32_t> Capture(SDL_Renderer*r){std::vector<uint32_t>p(640*360);assert(SDL_RenderReadPixels(r,nullptr,SDL_PIXELFORMAT_ARGB8888,p.data(),640*4)==0);return p;}
int main(){assert(SDL_Init(0)==0);int failures=0;
for(const std::string name:{"cut-x","cut-y","hole-count","hole-position","key-tolerance","key-grey","key-ring","plain-flags","nonplain-flags","unused-sprite-corner","source-pixels","failed-piece-upload","failed-crisp-upload"}){
 auto*a=SDL_CreateRGBSurfaceWithFormat(0,640,360,32,SDL_PIXELFORMAT_ARGB8888);auto*b=SDL_CreateRGBSurfaceWithFormat(0,640,360,32,SDL_PIXELFORMAT_ARGB8888);auto*ra=SDL_CreateSoftwareRenderer(a);auto*rb=SDL_CreateSoftwareRenderer(b);assert(a&&b&&ra&&rb);GpuCompositor reused(ra);
 std::vector<uint32_t>pixels(256*192,0xFF445566),world(640*360,0xFF112233);std::vector<uint8_t>flags(256*192,1);ScreenImage screens[2]={{pixels.data(),flags.data()},{pixels.data(),flags.data()}};WideImage wide{world.data(),640,360};Scene scene;Element el=Crop(0,0,0,32,24);el.crisp=true;
 if(name=="cut-y")el.cutX=0;
 if(name=="hole-count"||name=="hole-position"){scene.wide=true;if(name=="hole-position"){el.holeCount=1;el.holes[0][0]=0;el.holes[0][1]=0;el.holes[0][2]=8;el.holes[0][3]=8;}}
 if(name=="key-tolerance")el.keys[0]=0x445560;
 if(name=="key-grey"){std::fill(pixels.begin(),pixels.end(),0xFF404040);el.keyGrey=16;}
 if(name=="key-ring"){el.keys[0]=0x445566;el.keyRing=1;}
 if(name=="failed-piece-upload")std::fill(flags.begin(),flags.end(),0x10);
 if(name=="nonplain-flags")el.drop=LAYER_BG1;
 if(name=="unused-sprite-corner")el.drop=LAYER_OBJ;
 if(name=="plain-flags")for(size_t i=0;i<pixels.size();i++)pixels[i]=(i%2)?0xFF123456:0xFFEEDD22;
 scene.items={el};reused.Draw(scene,screens,640,360,false,scene.wide?&wide:nullptr);
 if(name=="cut-x"){el.cutX=8;el.cutY=0;}
 if(name=="cut-y")el.cutY=8;
 if(name=="hole-count"||name=="hole-position"){el.holeCount=1;el.holes[0][0]=8;el.holes[0][1]=4;el.holes[0][2]=24;el.holes[0][3]=20;}
 if(name=="key-tolerance")el.keyTolerance=8;
 if(name=="key-grey")el.keyGrey=80;
 if(name=="key-ring")el.keyRing=0;
 if(name=="plain-flags")std::fill(flags.begin(),flags.end(),0x40);
 if(name=="nonplain-flags")std::fill(flags.begin(),flags.end(),2);
 if(name=="unused-sprite-corner")el.dropSpriteCorner=3;
 if(name=="source-pixels"||name=="failed-piece-upload"||name=="failed-crisp-upload")std::fill(pixels.begin(),pixels.end(),0xFF1166AA);
 const int before=totalUploads;scene.items={el};if(name=="failed-piece-upload")failNthUpload=1;if(name=="failed-crisp-upload")failNthUpload=2;reused.Draw(scene,screens,640,360,false,scene.wide?&wide:nullptr);assert(failNthUpload==0);if(name=="unused-sprite-corner")assert(totalUploads-before==1);if(name=="failed-piece-upload"||name=="failed-crisp-upload")reused.Draw(scene,screens,640,360,false,scene.wide?&wide:nullptr);GpuCompositor cold(rb);cold.Draw(scene,screens,640,360,false,scene.wide?&wide:nullptr);
 if(Capture(ra)!=Capture(rb)){std::cerr<<"Cached crisp pixels stale after "<<name<<'\n';++failures;}
 SDL_DestroyRenderer(ra);SDL_DestroyRenderer(rb);SDL_FreeSurface(a);SDL_FreeSurface(b);
}
SDL_Quit();if(failures)return 1;std::cout<<"Crisp caches match fresh rendering after cuts, holes, key tolerance/grey/ring, flags and pixels change; failed base/crisp uploads retry.\n";return 0;}
