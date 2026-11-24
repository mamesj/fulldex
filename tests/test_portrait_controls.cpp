#include <cassert>
#include <cmath>
#include <SDL.h>
enum { PAD_A, PAD_B, PAD_X, PAD_Y, PAD_BACK, PAD_GUIDE, PAD_START, PAD_LS, PAD_RS, PAD_LB, PAD_RB, PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_COUNT };
#include "touchpad.h"
#include "layout.h"

int main() {
    TouchPad pad;
    for (auto size : {SDL_Point{360,640}, SDL_Point{720,1280}, SDL_Point{1080,2400}, SDL_Point{800,1280}, SDL_Point{360,380}}) {
        pad.Layout(size.x, size.y);
        assert(pad.HitButton(pad.dpadX, pad.dpadY) == TouchPad::DPAD);
        for (const auto& b : pad.buttons) assert(pad.HitButton(b.x, b.y) == b.pad);
        const int gameHeight = size.y - TouchPad::PortraitHeight(size.x, size.y);
        for (const auto& c : pad.Geometry()) {
            assert(c.x - c.rx >= 0 && c.x + c.rx <= size.x);
            assert(c.y - c.ry >= gameHeight && c.y + c.ry <= size.y);
        }
        Layout layout = LayoutFromName("vertical");
        SDL_Rect screens[2];
        LayoutRects(layout, size.x, gameHeight, screens);
        assert(screens[0].y + screens[0].h <= screens[1].y);
        assert(screens[1].y + screens[1].h <= gameHeight);
        for (const auto& screen : screens)
            for (int y=screen.y; y<screen.y+screen.h; y+=std::max(1,screen.h/5))
                for (int x=screen.x; x<screen.x+screen.w; x+=std::max(1,screen.w/5))
                    assert(pad.HitButton(x,y)==TouchPad::NONE);
        int x=-1,y=-1;
        assert(LayoutTouch(layout,screens,screens[1].x+screens[1].w/2,screens[1].y+screens[1].h/2,x,y));
        assert(std::abs(x-128)<=1 && std::abs(y-96)<=1);
    }
    pad.Layout(1280,720);
    assert(TouchPad::PortraitHeight(1280,720)==0);
    assert(std::fabs(pad.dpadR - 111.6f)<0.01f);
    for (const auto& b : pad.buttons) if(b.pad==PAD_LB || b.pad==PAD_RB) assert(b.y<100);
    pad.Layout(360,640); // Resizing back must restore the lower panel, not old landscape positions.
    for(const auto& b:pad.buttons) assert(b.y>450);
    std::puts("Portrait controls fit, hit their own actions and leave both screens touchable; landscape and resize preserved.");
}
