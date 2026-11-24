#include "merged.h"
#include <cassert>
#include <random>
#include <iostream>
inline void ReferenceBars(const ScreenImage& s, uint32_t left[192], uint32_t right[192], bool calm = false)
{
    const int reach = calm ? 40 : 8;
    const uint32_t keep = calm ? 170 : 256; // (in 256ths)
    for (int side = 0; side < 2; side++)
    {
        uint32_t rowSum[192][3];
        for (int y = 0; y < 192; y++)
        {
            uint32_t r = 0, g = 0, b = 0;
            for (int k = 0; k < 8; k++)
            {
                const uint32_t c = s.pixels[y * 256 + (side ? 255 - k : k)];
                r += (c >> 16) & 0xFF;
                g += (c >> 8) & 0xFF;
                b += c & 0xFF;
            }
            rowSum[y][0] = r;
            rowSum[y][1] = g;
            rowSum[y][2] = b;
        }
        uint32_t* out = side ? right : left;
        for (int y = 0; y < 192; y++)
        {
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (int k = std::max(0, y - reach); k <= std::min(191, y + reach); k++, n += 8)
            {
                r += rowSum[k][0];
                g += rowSum[k][1];
                b += rowSum[k][2];
            }
            out[y] = 0xFF000000 | (r / n * keep >> 8) << 16 | (g / n * keep >> 8) << 8 | (b / n * keep >> 8);
        }
    }
}

int main(){std::mt19937 rng(90217);std::vector<uint32_t> pixels(256*192);ScreenImage image{pixels.data(),nullptr};uint32_t a[192],b[192],c[192],d[192];
for(int t=0;t<152;t++){for(auto&v:pixels)v=t==150?0:t==151?0xFFFFFFFF:rng();for(bool calm:{false,true}){ReferenceBars(image,a,b,calm);MergedNarrowBars(image,c,d,calm);assert(std::equal(a,a+192,c));assert(std::equal(b,b+192,d));}}
std::cout<<"Sliding-window bars match every pixel for 304 random/black/white normal and calm cases, including clipped top/bottom windows.\n";}
