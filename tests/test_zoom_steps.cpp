#include "ds_zoom.h"
#include "../gba/frontend/pokedaisy/wide_camera.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main()
{
    DsZoom camera;
    const float requested[]={1.0f,1.1f,1.2f,1.3f,1.4f,1.5f,2.0f};
    for(int pass=0;pass<2;pass++)
        for(float factor:requested)
        {
            assert(std::fabs(camera.Factor(camera.level)-factor)<0.0001f);
            for(int frame=0;frame<240;frame++)camera.Update(false,0,0,false,1.0f/60);
            assert(std::fabs(camera.zoom-factor)<0.0021f);
            camera.Cycle();
        }
    assert(camera.level==0);
    camera.level=3;
    for(int frame=0;frame<240;frame++)camera.Update(false,1,-1,false,1.0f/60);
    assert(camera.lookX>0 && camera.lookY>0);
    for(int frame=0;frame<120;frame++)camera.Update(true,0,0,false,1.0f/60);
    assert(camera.zoom==1 && camera.lookX==0 && camera.lookY==0 && camera.level==3);
    for(int frame=0;frame<240;frame++)camera.Update(false,0,0,false,1.0f/60);
    assert(std::fabs(camera.zoom-1.3f)<0.0021f);
    for (auto size : { std::pair<int,int>{640,480}, {1280,720}, {1920,1080}, {720,720}, {720,1280} })
    {
        PkWideCamera::Camera gba;
        gba.viewW=size.first; gba.viewH=size.second;
        const auto steps=gba.Levels();
        for(int i=0;i<5;i++) assert(std::fabs(gba.Factor(i)-(1.0f+i*0.1f))<0.0001f);
        for(int i=1;i<(int)steps.size();i++) assert(steps[i]<steps[i-1]);
        for(int i=0;i<(int)steps.size();i++) { assert(gba.level==i); gba.Cycle(); }
        assert(gba.level==0);
        gba.level=3;
        int64_t now=1000000000;
        PkWideCamera::Window window;
        for(int frame=0;frame<240;frame++,now+=16666667) window=gba.Frame(PkWideCamera::SCENE_FIELD,now);
        assert(window.valid && window.w<=gba.MaxW && window.h<=gba.MaxH);
        assert(std::fabs(steps[0]/window.scale-1.3f)<0.0021f);
        for(int frame=0;frame<120;frame++,now+=16666667) window=gba.Frame(PkWideCamera::SCENE_TEXT,now);
        assert(std::fabs(window.scale-steps[0])<0.0021f && gba.level==3);
        for(int frame=0;frame<240;frame++,now+=16666667) window=gba.Frame(PkWideCamera::SCENE_FIELD,now);
        assert(std::fabs(steps[0]/window.scale-1.3f)<0.0021f);
    }
    for(auto size:{std::pair<int,int>{4800,900},{360,1600}})
    {
        auto steps=PkWideCamera::Camera::LevelsFor(size.first,size.second);
        for(size_t i=1;i<steps.size();i++) assert(size.first/steps[i]<=800 && size.second/steps[i]<=800);
    }
    std::cout<<"DS/GBA fine zoom steps, bounds, wraparound, easing and close/restore passed\n";
}
