
#include <SDL.h>
static bool failNextUpload = false;
static int UploadForTest(SDL_Texture* t,const SDL_Rect* rect,const void* pixels,int pitch)
{
    if (failNextUpload) { failNextUpload = false; return -1; }
    return SDL_UpdateTexture(t,rect,pixels,pitch);
}
#define SDL_UpdateTexture UploadForTest
#include "gpucompose.h"
#undef SDL_UpdateTexture
#include <cassert>
#include <iostream>
#include <random>
#include <memory>
static std::vector<uint32_t> Capture(SDL_Renderer*r,int w,int h){std::vector<uint32_t>p(w*h);assert(SDL_RenderReadPixels(r,nullptr,SDL_PIXELFORMAT_ARGB8888,p.data(),w*4)==0);return p;}
int main(){assert(SDL_Init(0)==0);auto*a=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_ARGB8888);auto*b=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_ARGB8888);assert(a&&b);auto*r=SDL_CreateSoftwareRenderer(a);auto*oracle=SDL_CreateSoftwareRenderer(b);assert(r&&oracle);GpuCompositor reuse(r),fresh(oracle);std::vector<uint32_t>screen(256*192,0xFF224466),world(640*360,0xFF887755);ScreenImage screens[2]={{screen.data(),nullptr},{screen.data(),nullptr}};Scene scene;scene.wide=true;scene.items={FullScreen(0)};WideImage wide{world.data(),640,360};wide.serial=1;
auto check=[&](int w=1280,int h=720){reuse.Draw(scene,screens,w,h,false,&wide);auto unknown=wide;unknown.serial=0;fresh.Draw(scene,screens,w,h,false,&unknown);assert(Capture(r,1280,720)==Capture(oracle,1280,720));};
check();assert(reuse.WideUploads==1);check();assert(reuse.WideUploads==1);
wide.fadeMode=1;wide.fade=8;check();assert(reuse.WideUploads==1);wide.fadeMode=2;wide.fade=9;check();assert(reuse.WideUploads==1);
wide.fadeMode=wide.fade=0;std::fill(world.begin(),world.end(),0xFF1122EE);++wide.serial;check();assert(reuse.WideUploads==2);
// A copied pipeline buffer has the same published pixels and serial.
auto copy=world;wide.pixels=copy.data();check();assert(reuse.WideUploads==2);
// The DS screen's coverage changes the filled holes even for the same world.
check(1024,720);assert(reuse.WideUploads==3);check(1280,720);assert(reuse.WideUploads==4);
// Untracked producers must upload on every frame, including returning to an older serial.
wide.serial=0;check();check();assert(reuse.WideUploads==6);wide.serial=2;check();assert(reuse.WideUploads==7);
// Battle placement is deliberately uncached even if a caller supplies a serial.
wide.placed=true;wide.baseX=-32;check();check();assert(reuse.WideUploads==9);wide.placed=false;check();assert(reuse.WideUploads==10);
// Texture reallocation with a held serial must not reuse old dimensions.
wide.w=320;wide.h=180;check();assert(reuse.WideUploads==11);
// Transparent outside rows and actual fills retain the original composed picture.
wide.w=640;wide.h=360;wide.pixels=world.data();for(int y=0;y<360;y++)for(int x=0;x<640;x++)world[y*640+x]=(y%5==0||x<8||x>=632)?0:0xFF998877;++wide.serial;check();check();assert(reuse.WideUploads==12);
// A failed upload is never recorded as valid; the next draw retries it.
++wide.serial;failNextUpload=true;reuse.Draw(scene,screens,1280,720,false,&wide);assert(reuse.WideUploads==13);check();assert(reuse.WideUploads==14);check();assert(reuse.WideUploads==14);
std::cout<<"Repeated worlds skip uploads; changed serial, fill bounds, size, untracked/battle images and fades match uncached pixels.\n";
SDL_DestroyRenderer(r);SDL_DestroyRenderer(oracle);SDL_FreeSurface(a);SDL_FreeSurface(b);SDL_Quit();}
