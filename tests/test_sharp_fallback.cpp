
#include <SDL.h>
static bool failCopy=false,failQuery=false;
static SDL_Renderer* withoutTargets=nullptr;
static SDL_bool TargetsForTest(SDL_Renderer*r){return r==withoutTargets?SDL_FALSE:SDL_RenderTargetSupported(r);}
static int CopyForTest(SDL_Renderer*r,SDL_Texture*t,const SDL_Rect*a,const SDL_Rect*b){if(failCopy&&SDL_GetRenderTarget(r)){failCopy=false;return -1;}return SDL_RenderCopy(r,t,a,b);}
static int QueryForTest(SDL_Texture*t,Uint32*f,int*a,int*w,int*h){if(failQuery){failQuery=false;return -1;}return SDL_QueryTexture(t,f,a,w,h);}
#define SDL_RenderTargetSupported TargetsForTest
#define SDL_RenderCopy CopyForTest
#define SDL_QueryTexture QueryForTest
#include "gpucompose.h"
#undef SDL_RenderTargetSupported
#undef SDL_RenderCopy
#undef SDL_QueryTexture
#include <cassert>
#include <iostream>
#include <string>
static std::vector<uint32_t>Capture(SDL_Renderer*r){std::vector<uint32_t>p(640*360);assert(SDL_RenderReadPixels(r,nullptr,SDL_PIXELFORMAT_ARGB8888,p.data(),640*4)==0);return p;}
int main(int argc,char**argv){assert(argc==2);const std::string mode=argv[1];for(int alpha:{255,128}){assert(SDL_Init(0)==0);auto*a=SDL_CreateRGBSurfaceWithFormat(0,640,360,32,SDL_PIXELFORMAT_ARGB8888);auto*b=SDL_CreateRGBSurfaceWithFormat(0,640,360,32,SDL_PIXELFORMAT_ARGB8888);auto*ra=SDL_CreateSoftwareRenderer(a);auto*rb=SDL_CreateSoftwareRenderer(b);assert(a&&b&&ra&&rb);assert(SDL_RenderTargetSupported(ra));GpuCompositor reused(ra),cold(rb);std::vector<uint32_t>pixels(256*192,0xFF224466);std::vector<uint8_t>flags(256*192,0x10);ScreenImage screens[2]={{pixels.data(),flags.data()},{pixels.data(),flags.data()}};Scene scene;scene.items={FullScreen(0)};scene.items[0].alpha=alpha;
reused.Draw(scene,screens,640,360,false,nullptr);std::fill(pixels.begin(),pixels.end(),0xFFCC6633);failCopy=mode=="copy";failQuery=mode=="query";reused.Draw(scene,screens,640,360,false,nullptr);assert(!failCopy&&!failQuery);withoutTargets=rb;cold.Draw(scene,screens,640,360,false,nullptr);if(Capture(ra)!=Capture(rb)){std::cerr<<"Sharp "<<mode<<" failure drew stale intermediate pixels instead of the fresh source.\n";return 1;}assert(SDL_GetRenderTarget(ra)==nullptr);withoutTargets=nullptr;reused.Draw(scene,screens,640,360,false,nullptr);cold.Draw(scene,screens,640,360,false,nullptr);assert(Capture(ra)==Capture(rb));SDL_DestroyRenderer(ra);SDL_DestroyRenderer(rb);SDL_FreeSurface(a);SDL_FreeSurface(b);SDL_Quit();}std::cout<<"Sharp "<<mode<<" failure uses fresh source, preserves opaque/translucent blending and recovers next frame.\n";return 0;}
