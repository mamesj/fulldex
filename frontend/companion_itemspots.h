#pragma once
// Where each item is found in HeartGold, SoulSilver and hacks of them (HG-Engine's included): given
// by someone, an item ball or hidden, read from each map's own scripts and events in the player's
// ROM. Formats from pokeheartgold (include/map_events_internal.h, the std scripts) and hg-engine
// (tools/source/dumptools/scrcmd.json: setvar 0x29, callstd 0x14, giveitem 0x7D, hasspaceforitem
// 0x7F; asm/include/scriptmacros.inc: std_obtain_item_verbose 2008, std_give_item_verbose 2033;
// item balls 7000 on, hidden items 8000 on).
// No game data is bundled or written to disk.
#include <algorithm>
#include <vector>
namespace Companion
{
struct ItemSpot {int place=-1,how=0,quantity=1;}; // (how: 0 given, 1 an item ball, 2 hidden)
struct ItemSpotMap {int place=-1,scripts=-1,events=-1;};
inline std::vector<std::vector<ItemSpot>> FindItemSpots(const std::vector<ItemSpotMap>& maps,const std::vector<Bytes>& scripts,
    const std::vector<Bytes>& events,const std::vector<Bytes>& addons,size_t itemCount,const Bytes& arm9={})
{
    std::vector<std::vector<ItemSpot>> out(itemCount);
    auto add=[&](unsigned item,int place,int how,unsigned quantity)
    {
        if(!item||item>=itemCount||place<0)return;
        quantity=std::clamp(quantity,1u,999u);
        for(auto& s:out[item])if(s.place==place&&s.how==how){s.quantity=std::max<int>(s.quantity,quantity);return;}
        out[item].push_back({place,how,(int)quantity});
    };
    auto u16=[](const Bytes& f,size_t at){return (unsigned)U16(f.data()+at);};
    // (A script file: its scripts' offsets, each relative to the end of its own, until 0xFD13.)
    auto starts=[&](const Bytes& f)
    {
        std::vector<size_t> at;
        for(size_t o=0;Range(f.size(),o,4)&&u16(f,o)!=0xFD13&&at.size()<8192;o+=4)at.push_back(o+4+U32(f.data()+o));
        return at;
    };
    // The item balls' scripts: the file whose scripts each begin setvar 0x8008, the item (then
    // setvar 0x8009, how many).
    std::vector<unsigned> ballItem,ballCount;
    for(const auto& f:scripts)
    {
        const auto at=starts(f);
        if(at.size()<100)continue;
        size_t good=0;
        for(size_t a:at)if(Range(f.size(),a,6)&&u16(f,a)==0x29&&u16(f,a+2)==0x8008)good++;
        if(good*10<at.size()*9)continue;
        for(size_t a:at)
        {
            const bool item=Range(f.size(),a,12)&&u16(f,a)==0x29&&u16(f,a+2)==0x8008;
            ballItem.push_back(item?u16(f,a+4):0);
            ballCount.push_back(item&&u16(f,a+6)==0x29&&u16(f,a+8)==0x8009?u16(f,a+10):1);
        }
        break;
    }
    // Hidden items: {u16 item, u8 quantity, u8, u16, u16 index} each; the index is the hidden item's
    // script, less 8000. (HG-Engine keeps them with its code addons; HeartGold in its ARM9.)
    const Bytes* hidden=nullptr;
    for(const auto& m:addons)
    {
        if(m.size()<8*50||m.size()%8)continue;
        bool ok=true;std::vector<bool> used(65536);
        for(size_t e=0;e<m.size()&&ok;e+=8)
        {
            const unsigned item=u16(m,e),index=u16(m,e+6);
            ok=item&&item<itemCount&&m[e+2]>=1&&m[e+2]<=99&&!used[index];
            if(ok)used[index]=true;
        }
        if(ok){hidden=&m;break;}
    }
    // (HeartGold and SoulSilver's own: the same records in the ARM9, 231 of them in a row.)
    Bytes ownTable;
    if(!hidden&&!arm9.empty())
        for(size_t start=0;Range(arm9.size(),start,8*100)&&ownTable.empty();start+=2)
        {
            size_t n=0;std::vector<bool> used(65536);
            for(;Range(arm9.size(),start+n*8,8);n++)
            {
                const unsigned item=u16(arm9,start+n*8),index=u16(arm9,start+n*8+6),count=arm9[start+n*8+2];
                if(!item||item>=itemCount||count<1||count>99||used[index])break;
                used[index]=true;
            }
            if(n>=100)ownTable.assign(arm9.begin()+start,arm9.begin()+start+n*8);
        }
    if(!hidden&&!ownTable.empty())hidden=&ownTable;
    std::vector<std::pair<int,int>> done; // ({file, place}: each file read once for each place)
    auto first=[&](int file,int place){const std::pair<int,int> key{file,place};if(std::find(done.begin(),done.end(),key)!=done.end())return false;done.push_back(key);return true;};
    for(const auto& map:maps)
    {
        if(map.place<0)continue;
        // Given: setvar 0x8004, the item; setvar 0x8005, how many; then std_give_item_verbose or
        // hasspaceforitem first. Or giveitem with the item and how many.
        if(map.scripts>=0&&map.scripts<(int)scripts.size()&&first(map.scripts,map.place))
        {
            const Bytes& f=scripts[map.scripts];
            for(size_t i=0;Range(f.size(),i,14);i++)
            {
                if(u16(f,i)==0x29&&u16(f,i+2)==0x8004&&u16(f,i+6)==0x29&&u16(f,i+8)==0x8005)
                {
                    // (Then hasspaceforitem, or std_give_item_verbose (2033) or std_obtain_item_verbose
                    // (2008) a few commands on, before another item is set; not takeitem or hasitem.)
                    const unsigned next=u16(f,i+12);
                    bool gift=next==0x7F;
                    for(size_t k=i+12;!gift&&next!=0x7E&&next!=0x80&&Range(f.size(),k,4)&&k<i+12+32;k++)
                    {
                        if(u16(f,k)==0x29&&u16(f,k+2)==0x8004)break;
                        gift=u16(f,k)==0x14&&(u16(f,k+2)==2033||u16(f,k+2)==2008);
                    }
                    if(gift)add(u16(f,i+4),map.place,0,u16(f,i+10));
                }
                if(u16(f,i)==0x7D)
                {
                    const unsigned item=u16(f,i+2),n=u16(f,i+4),var=u16(f,i+6);
                    if(n>=1&&n<=99&&var>=0x8000&&var<=0x8010)add(item,map.place,0,n);
                }
            }
        }
        // Item balls and hidden items: the map's people and things whose scripts are theirs.
        if(map.events>=0&&map.events<(int)events.size()&&first(-1-map.events,map.place))
        {
            const Bytes& f=events[map.events];
            if(!Range(f.size(),0,4))continue;
            const size_t bg=U32(f.data());
            if(bg>1024||!Range(f.size(),4,bg*20+4))continue;
            for(size_t i=0;i<bg&&hidden;i++)
            {
                const unsigned script=u16(f,4+i*20);
                if(script<8000||script>=8000+65536)continue;
                for(size_t e=0;e<hidden->size();e+=8)
                    if(u16(*hidden,e+6)==script-8000){add(u16(*hidden,e),map.place,2,(*hidden)[e+2]);break;}
            }
            const size_t objects=U32(f.data()+4+bg*20),at=8+bg*20;
            if(objects>1024||!Range(f.size(),at,objects*32))continue;
            for(size_t i=0;i<objects;i++)
            {
                const unsigned script=u16(f,at+i*32+10);
                if(script>=7000&&script<7000+ballItem.size())add(ballItem[script-7000],map.place,1,ballCount[script-7000]);
            }
        }
    }
    return out;
}
}
