#pragma once
// Read a supported cart's own names and dex data; none is bundled or written out.
// Text decoding adapted from PokeDaisy Gen4Text/Gen5Text/NdsArt (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "companion_icons.h"
#include "companion_data.h"
#include "companion_art.h"
#include "companion_dex.h"
#include "companion_guides.h"
#include <deque>
#include <map>
#include <sstream>
#include <cctype>
#include <set>
#include <cstring>
namespace Companion
{
// (A file of up to 96 MB: an HG-Engine hack's battle pictures, one set for each of its 1,500 species
// and forms, are about 40 MB.)
inline Bytes RomFile(const Bytes& rom,std::string path)
{
    if(rom.size()<0x200)return {};
    if(!path.empty()&&path[0]=='/')path.erase(0,1);
    uint32_t fnt=U32(rom.data()+0x40),fntSize=U32(rom.data()+0x44),fat=U32(rom.data()+0x48),fatSize=U32(rom.data()+0x4C);
    if(fntSize>0x400000 || !Range(rom.size(),fnt,fntSize)||!Range(rom.size(),fat,fatSize))return {};
    uint16_t dir=0xF000;
    for(int depth=0;depth<32;depth++)
    {
        size_t entry=(dir&0xFFF)*8,slash=path.find('/');
        if(!Range(fntSize,entry,8))return {};
        size_t pos=U32(rom.data()+fnt+entry);uint32_t id=U16(rom.data()+fnt+entry+4);
        bool descended=false;
        while(pos<fntSize)
        {
            int tag=rom[fnt+pos++];if(!tag)break;
            size_t n=tag&127;if(!Range(fntSize,pos,n))return {};
            std::string name((const char*)rom.data()+fnt+pos,n);pos+=n;
            if(tag&128){if(!Range(fntSize,pos,2))return {};uint16_t sub=U16(rom.data()+fnt+pos);pos+=2;if(slash!=std::string::npos && name==path.substr(0,slash)){dir=sub;path=path.substr(slash+1);descended=true;break;}}
            else {if(slash==std::string::npos && name==path){if(!Range(fatSize,(size_t)id*8,8))return {};uint32_t begin=U32(rom.data()+fat+id*8),end=U32(rom.data()+fat+id*8+4);if(end<begin||end-begin>96*1024*1024||!Range(rom.size(),begin,end-begin))return {};return Bytes(rom.begin()+begin,rom.begin()+end);}id++;}
        }
        if(!descended)return {};
    }
    return {};
}
// (keep: which members to copy; the others are left empty, as a big archive's unused ones.)
inline std::vector<Bytes> Narc(const Bytes& data,bool (*keep)(size_t)=nullptr)
{
    std::vector<Bytes> result;
    if(data.size()<16||memcmp(data.data(),"NARC",4))return result;
    size_t pos=U16(data.data()+12),fat=0,img=0,imgSize=0;uint32_t count=0;
    for(int i=0;i<16 && Range(data.size(),pos,8);i++)
    {
        uint32_t len=U32(data.data()+pos+4);if(len<8||!Range(data.size(),pos,len))return {};
        if(!memcmp(data.data()+pos,"BTAF",4)){if(len<12)return {};count=U16(data.data()+pos+8);fat=pos+12;if(count>32768||!Range(len,12,(size_t)count*8))return {};}
        if(!memcmp(data.data()+pos,"GMIF",4)){img=pos+8;imgSize=len-8;}
        pos+=len;
    }
    if(!fat||!img)return {};
    for(uint32_t i=0;i<count;i++){uint32_t a=U32(data.data()+fat+i*8),b=U32(data.data()+fat+i*8+4);if(b<a||!Range(imgSize,a,b-a))return {};if(keep&&!keep(i)){result.emplace_back();continue;}result.emplace_back(data.begin()+img+a,data.begin()+img+b);}
    return result;
}
inline std::vector<std::string> Text(const Bytes& file,bool gen5)
{
    std::vector<std::string> result;
    if(file.size()<16)return result;
    const int count=U16(file.data()+(gen5?2:0));if(count>8192)return result;
    size_t section=gen5?U32(file.data()+12):0;
    if(!Range(file.size(),gen5?section+4:4,(size_t)count*8))return result;
    for(int i=0;i<count;i++)
    {
        size_t e=(gen5?section+4:4)+i*8,offset=0,length=0;
        if(gen5){offset=section+U32(file.data()+e);length=U16(file.data()+e+4);}
        else{uint32_t key=(765u*(i+1)*U16(file.data()+2))&65535;key|=key<<16;offset=U32(file.data()+e)^key;length=U32(file.data()+e+4)^key;}
        if(length>16384||!Range(file.size(),offset,length*2)){result.emplace_back();continue;}
        std::vector<uint16_t> codes(length);
        if(gen5){uint16_t key=length?U16(file.data()+offset+(length-1)*2)^0xFFFF:0;for(int j=(int)length-1;j>=0;j--){codes[j]=U16(file.data()+offset+j*2)^key;key=(key>>3)|(key<<13);}}
        else{uint16_t key=(i+1)*596947u;for(size_t j=0;j<length;j++){codes[j]=U16(file.data()+offset+j*2)^key;key+=18749;}}
        std::string line;
        for(size_t j=0;j<codes.size();j++)
        {
            int c=codes[j];if(c==0xFFFF)break;
            if((gen5&&c==0xF000)||(!gen5&&c==0xFFFE)){if(!Range(codes.size(),j,3))break;j+=2+codes[j+2];continue;}
            if(gen5&&c==0xFFFE){line+=' ';continue;}
            if(gen5&&c==0xF100){uint32_t bits=0;int have=0;bool end=false;for(size_t k=j+1;k<codes.size()&&!end;k++){bits|=(uint32_t)codes[k]<<have;have+=16;while(have>=9){int ch=bits&511;bits>>=9;have-=9;if(ch==511){end=true;break;}line+=AsciiChar(ch,true);}}break;}
            // (Gen 4's compressed strings, as its trainers' names: 9-bit characters in the low 15 bits
            // of each unit, hg-engine's msgenc DecodeTrainerNameMessage.)
            if(!gen5&&c==0xF100){uint32_t bits=0;int have=0;bool end=false;for(size_t k=j+1;k<codes.size()&&!end;k++){bits|=(uint32_t)(codes[k]&0x7FFF)<<have;have+=15;while(have>=9){int ch=bits&511;bits>>=9;have-=9;if(ch==511){end=true;break;}line+=AsciiChar(ch,false);}}break;}
            line+=AsciiChar(c,gen5);
        }
        while(!line.empty()&&line.back()==' ')line.pop_back();
        result.push_back(line);
    }
    return result;
}
// Decode the ARM9's BLZ segment in memory. Format adapted from PokeDaisy gen_gen5_tables.py.
inline Bytes UnpackBlz(const Bytes& input)
{
    if(input.size()<8)return {};
    size_t n=input.size(),word=U32(input.data()+n-8),extra=U32(input.data()+n-4);
    if(!extra)return input;
    size_t header=word>>24,packed=word&0xFFFFFF;
    if(header<8||packed<header||packed>n||extra>0x400000||n>0x400000-extra)return {};
    Bytes out=input;out.resize(n+extra);size_t start=n-packed,ip=n-header,op=out.size();
    while(ip>start&&op>start)
    {
        unsigned flags=out[--ip];
        for(int bit=0;bit<8&&ip>start&&op>start;bit++,flags<<=1)
        {
            if(flags&128)
            {
                if(ip-start<2)return {};
                ip-=2;
                size_t distance=(((out[ip+1]&15)<<8)|out[ip])+3,length=(out[ip+1]>>4)+3;
                if(length>op-start)return {};
                for(size_t i=0;i<length;i++){--op;if(distance>=out.size()-op)return {};out[op]=out[op+distance];}
            }
            else {if(op<=start)return {};out[--op]=out[--ip];}
        }
    }
    return op==start?out:Bytes{};
}
// ARM9 overlay [n] as the game loads it (unpacked when its flags say so).
inline Bytes RomOverlay(const Bytes& rom,unsigned n)
{
    if(rom.size()<0x200)return {};
    size_t table=U32(rom.data()+0x50),tableSize=U32(rom.data()+0x54),fat=U32(rom.data()+0x48);
    if(n>=tableSize/32||!Range(rom.size(),table+n*32,32))return {};
    const uint8_t* e=rom.data()+table+n*32;uint32_t id=U32(e+24),flags=U32(e+28);
    if(!Range(rom.size(),fat+(size_t)id*8,8))return {};
    uint32_t begin=U32(rom.data()+fat+id*8),end=U32(rom.data()+fat+id*8+4);
    if(end<begin||!Range(rom.size(),begin,end-begin))return {};
    Bytes data(rom.begin()+begin,rom.begin()+end);
    return flags&0x01000000?UnpackBlz(data):data;
}
inline Bytes RomArm9(const Bytes& rom)
{
    if(rom.size()<0x200)return {};
    size_t offset=U32(rom.data()+0x20),size=U32(rom.data()+0x2C);uint32_t ram=U32(rom.data()+0x28);
    if(size>0x400000||!Range(rom.size(),offset,size))return {};
    Bytes arm(rom.begin()+offset,rom.begin()+offset+size);
    // Nitro's module-parameter marker has opposite word order in these Gen4 and Gen5 binaries.
    const unsigned char magic4[]={0x21,0x06,0xC0,0xDE,0xDE,0xC0,0x06,0x21};
    const unsigned char magic5[]={0xDE,0xC0,0x06,0x21,0x21,0x06,0xC0,0xDE};
    auto at=std::search(arm.begin(),arm.end(),std::begin(magic4),std::end(magic4));
    if(at==arm.end())at=std::search(arm.begin(),arm.end(),std::begin(magic5),std::end(magic5));
    if(at==arm.end())return arm;
    size_t marker=at-arm.begin();if(marker<28)return {};
    uint32_t end=U32(arm.data()+marker-8);if(!end)return arm;
    if(end<ram||end-ram>arm.size())return {};
    size_t length=end-ram;auto unpacked=UnpackBlz(Bytes(arm.begin(),arm.begin()+length));
    if(unpacked.empty()||unpacked.size()+arm.size()-length>0x400000)return {};
    unpacked.insert(unpacked.end(),arm.begin()+length,arm.end());return unpacked;
}
}
#include "companion_hge.h"
#include "companion_itemspots.h"
namespace Companion
{
struct Metadata
{
    std::vector<std::string> species,moves,items,descriptions,abilities,itemDescriptions;
    // (HG-Engine hacks, as Heartfelt: their species, forms, moves and tables; companion_hge.h.)
    HgEngine engine;
    bool unreadable=false; // (the ROM's text not where the game keeps it: see Load)
    // (HeartGold and SoulSilver: each item's pocket (7: key items), and where each is found.)
    std::vector<int> itemPockets;
    std::vector<std::vector<ItemSpot>> itemSpots;
    const std::vector<ItemSpot>& ItemSpots(int item)const{static const std::vector<ItemSpot> none;return item>0&&item<(int)itemSpots.size()?itemSpots[item]:none;}
    static const char* SpotHow(int how){return how==1?"ITEM BALL":how==2?"HIDDEN":"GIVEN";}
    std::vector<Bytes> personal,evolutions,learnsets,moveRecords;
    // (The Pokedex page: companion_dex.h.)
    std::vector<uint16_t> machineMoves;
    std::vector<std::vector<uint16_t>> eggMoves;
    std::vector<Bytes> battlePics,dexSizes;
    std::vector<uint16_t> tutorMoves;
    std::vector<uint64_t> tutorBits; // (by species: bit i, tutorMoves[i])
    std::vector<uint16_t> Tutors(int species)const
    {
        std::vector<uint16_t> out;
        if(engine.on&&!engine.tutorBits.empty())
        {
            const uint8_t* row=HgEngine::Row(engine.tutorBits,engine.tutorStride,species);
            if(row)for(size_t i=0;i<engine.tutors.size();i++)if(row[i/8]>>(i%8)&1)out.push_back(engine.tutors[i]);
            if(out.empty()&&engine.UsedForm(species))return Tutors(engine.Base(species));
            return out;
        }
        if(species<=0||species>=(int)tutorBits.size())return out;
        for(size_t i=0;i<tutorMoves.size()&&i<64;i++)if(tutorBits[species]>>i&1)out.push_back(tutorMoves[i]);
        if(out.empty()&&engine.UsedForm(species))return Tutors(engine.Base(species));
        return out;
    }
    DexStats Stats(int species)const{return species>0&&species<(int)personal.size()?PersonalStats(personal[species],gen5):DexStats{};}
    // Height in tenths of a metre, weight in tenths of a kilogram; false when unknown.
    bool Size(int species,int& height,int& weight)const
    {
        if(gen5){if(species<=0||species>=(int)personal.size()||personal[species].size()<0x28)return false;height=U16(personal[species].data()+0x24);weight=U16(personal[species].data()+0x26);return true;}
        if(engine.on&&species>493)return engine.Size(species,height,weight);
        if(dexSizes.size()<2||!Range(dexSizes[0].size(),(size_t)species*4,4)||!Range(dexSizes[1].size(),(size_t)species*4,4))return false;
        height=(int)U32(dexSizes[0].data()+species*4);weight=(int)U32(dexSizes[1].data()+species*4);return true;
    }
    // Each machine this species can learn: its index in machineMoves.
    std::vector<int> Machines(int species)const
    {
        std::vector<int> out;
        if(species<=0||species>=(int)personal.size())return out;
        if(engine.on&&!engine.machines.empty())
        {
            // (HG-Engine: its own table of bits; the TMs in order, then the HMs.)
            // (The TRs and Omega Ruby's HM07 only when the game gives them somewhere: Heartfelt Gold
            // has their items but hands out none of them.)
            const uint8_t* row=HgEngine::Row(engine.machineBits,engine.machineStride,species);
            if(!row)return out;
            auto shown=[&](int i){return !engine.machineLabels[i].empty()&&((i!=100&&i<240)||!ItemSpots(engine.machineItems[i]).empty());};
            // (The TMs by their numbers, then the HMs, then any TRs.)
            std::vector<int> order;
            for(int i=0;i<(int)engine.machines.size();i++)if(i<92||(i>=101&&i<240))order.push_back(i);
            std::stable_sort(order.begin(),order.end(),[&](int a,int b){return atoi(engine.machineLabels[a].c_str()+2)<atoi(engine.machineLabels[b].c_str()+2);});
            for(int i=92;i<=100&&i<(int)engine.machines.size();i++)order.push_back(i);
            for(int i=240;i<(int)engine.machines.size();i++)order.push_back(i);
            for(int i:order)if(row[i/8]>>(i%8)&1&&shown(i))out.push_back(i);
            if(out.empty()&&engine.UsedForm(species))return Machines(engine.Base(species));
            return out;
        }
        for(int i=0;i<(int)machineMoves.size();i++)if(CanLearnMachine(personal[species],gen5,i))out.push_back(i);
        if(out.empty()&&engine.UsedForm(species))return Machines(engine.Base(species));
        return out;
    }
    // "TM06" / "HM01" for the machine at [index]; an HG-Engine hack's its item's own name ("TM006").
    std::string MachineName(int index)const
    {
        if(engine.on&&!engine.machines.empty())return index>=0&&index<(int)engine.machineLabels.size()?engine.machineLabels[index]:std::string();
        return MachineLabel(gen5,index);
    }
    const std::vector<uint16_t>& EggMoves(int species)const{static const std::vector<uint16_t> none;return species>0&&species<(int)eggMoves.size()?eggMoves[species]:none;}
    MapImage Picture(int species,bool shiny,bool female=false)const{return BattlePicture(battlePics,gen5,species,shiny,female);}
    bool gen5=false;
    MapImage regionMap;
    // (Party icons: the ROM's icon archive and its palettes, HGSS and Platinum; companion_icons.h.)
    std::vector<Bytes> monIcons;
    std::vector<std::vector<uint32_t>> iconPalettes;
    // (Gen 5: the icons are every other member from the first NCGR (7 in Black and White, 8 in Black 2
    // and White 2), one for each species; their palettes chosen by a byte for each
    // species in the ARM9 (low nibble; high: its female icon's), found by Gen 4's table, which it repeats.)
    std::vector<uint8_t> gen5IconPalettes;
    int gen5IconStart=-1;
    void FindGen5IconPalettes(const Bytes& arm)
    {
        const size_t n=CompanionIcons::Gen4Egg;
        for(size_t o=0;o+n+700<arm.size();o++)
        {
            bool ok=true;
            for(size_t sp=1;sp<n&&ok;sp++){const uint8_t b=arm[o+sp];ok=(b&15)==CompanionIcons::Gen4Palette[sp]&&(b>>4)==(b&15);}
            if(!ok)continue;
            // (After Gen 4's: a Pokemon with a female icon has its palette in the high nibble.)
            size_t end=o+1;while(end<arm.size()&&(arm[end]&15)<=2&&(arm[end]>>4)<=2)end++;
            for(size_t q=o;q<end;q++)gen5IconPalettes.push_back(arm[q]&15);
            return;
        }
    }
    bool MonIcon(int species,bool egg,std::vector<uint32_t>& out)const
    {
        if(gen5)
        {
            const int member=gen5IconStart+2*species;
            if(egg||iconPalettes.empty()||gen5IconStart<0||species<1||species>=(int)gen5IconPalettes.size()||member>=(int)monIcons.size())return false;
            const int pal=gen5IconPalettes[species];
            return pal<(int)iconPalettes.size()&&CompanionIcons::Icon(monIcons[member],iconPalettes[pal],out);
        }
        if(iconPalettes.empty())return false;
        if(engine.on)
        {
            // (HG-Engine: an icon for each species and form after the archive's first 7, each
            // one's palette from its own table.)
            const int id=egg?CompanionIcons::Gen4Egg:species;
            if(id<1||id>=(int)engine.iconPalette.size()||7+id>=(int)monIcons.size())return false;
            const int pal=engine.iconPalette[id];
            return pal<(int)iconPalettes.size()&&CompanionIcons::Icon(monIcons[7+id],iconPalettes[pal],out);
        }
        const int id=egg?CompanionIcons::Gen4Egg:species;
        if(id<1||id>CompanionIcons::Gen4Egg||7+id>=(int)monIcons.size())return false;
        const int pal=CompanionIcons::Gen4Palette[id];
        if(pal>=(int)iconPalettes.size())return false;
        return CompanionIcons::Icon(monIcons[7+id],iconPalettes[pal],out);
    }
    // (Item icons, HGSS and Platinum: the ROM's item icon archive, and the ARM9's table of each
    // item's {data, icon, palette, Gen 3 id}, found by its shape (item i's data file is i).)
    std::vector<Bytes> itemIcons;
    std::vector<std::array<int,2>> itemIconIds;
    std::vector<int> itemDataIds; // (each item's data file, from the same table: HeartGold's not item i's file i past the TMs)
    bool ItemIcon(int item,std::vector<uint32_t>& out)const
    {
        if(item<=0||item>=(int)itemIconIds.size())return false;
        const auto ids=itemIconIds[item];
        if(ids[0]>=(int)itemIcons.size()||ids[1]>=(int)itemIcons.size())return false;
        const auto pals=CompanionIcons::Palettes(itemIcons[ids[1]]);
        return !pals.empty()&&CompanionIcons::Icon(itemIcons[ids[0]],pals[0],out);
    }
    void LoadItemIcons(const Bytes& rom,const Bytes& arm,bool hg)
    {
        itemIcons=Narc(RomFile(rom,gen5?"a/0/2/5":hg?"a/0/1/8":"itemtool/itemdata/item_icon.narc"));
        const int n=(int)itemIcons.size();
        if(n<2||arm.size()<8*64)return;
        auto u16=[&](size_t o){return (int)U16(arm.data()+o);};
        if(gen5)
        {
            // (Gen 5: {icon, palette} for each item, four bytes; found by item 1 (the Master Ball)
            // being the archive's first icon and palette, and the next ones being an icon and a
            // palette each.)
            auto is=[&](int m,const char* magic){return m>=0&&m<n&&itemIcons[m].size()>4&&!memcmp(itemIcons[m].data(),magic,4);};
            for(size_t o=0;o+4*64<=arm.size();o+=4)
            {
                if(u16(o+4)!=2||u16(o+6)!=3)continue;
                bool ok=true;
                for(int i=1;i<60&&ok;i++)ok=is(u16(o+4*i),"RGCN")&&is(u16(o+4*i+2),"RLCN");
                if(!ok)continue;
                for(size_t e=o;e+4<=arm.size()&&itemIconIds.size()<std::max<size_t>(items.size(),1);e+=4)
                {
                    if(!is(u16(e),"RGCN")||!is(u16(e+2),"RLCN"))break;
                    itemIconIds.push_back({u16(e),u16(e+2)});
                }
                return;
            }
            return;
        }
        for(size_t o=0;o+8*64<=arm.size();o+=4)
        {
            if(u16(o)!=0||u16(o+2)>=n||u16(o+4)>=n)continue;
            bool ok=true;
            for(int i=1;i<60&&ok;i++){const size_t e=o+8*i;ok=u16(e)==i&&u16(e+2)<n&&u16(e+4)<n;}
            if(!ok)continue;
            for(size_t e=o;e+8<=arm.size()&&itemIconIds.size()<std::max<size_t>(items.size(),1);e+=8)
            {
                if(u16(e+2)>=n||u16(e+4)>=n)break;
                itemIconIds.push_back({u16(e+2),u16(e+4)});itemDataIds.push_back(u16(e));
            }
            return;
        }
    }
    std::vector<std::string> places;
    std::vector<int> mapPlaces;
    struct MapRecord {int place=-1,matrix=-1,worldX=-1,worldY=-1,encounter=-1,scripts=-1,events=-1;};
    std::vector<MapRecord> mapRecords;
    std::vector<Bytes> wildRecords;
    std::vector<std::pair<int,int>> placePoints;
    bool hgss=false;
    std::vector<std::string> trainerNames;
    std::vector<Bytes> trainerData,trainerTeams;
    static std::string Name(const std::vector<std::string>& names,int id,const char* fallback)
    {return id>=0 && id<(int)names.size()&&!names[id].empty()?names[id]:std::string(fallback)+" #"+std::to_string(id);}
    std::string Species(int id)const{return engine.IsForm(id)?FormName(id):Name(species,id,"SPECIES");}
    // (Base stat total: the Megas' are their species' + 100.)
    int StatTotal(int id)const{int t=0;if(id>=0&&id<(int)personal.size()&&personal[id].size()>=6)for(int i=0;i<6;i++)t+=personal[id][i];return t;}
    // An HG-Engine form's name. The engine names none, so its species' with what kind of form it
    // is: a Mega Evolution (its stats 100 more than its species', as every Mega's; X and Y by their
    // Mega Stones' names when there are two), a Primal Reversion (Kyogre's and Groudon's), or which
    // of its species' forms it is.
    std::string FormName(int id)const
    {
        const int base=engine.Base(id);
        if(base==id)return Name(species,id,"SPECIES"); // (an unused one: no species)
        const auto forms=engine.Forms(base);
        const int n=FormIndex(id,base);
        std::string label=BareFormName(id,base);
        // (Two of a species' forms with one name, as Mega Meowstic's male and female: their numbers too.)
        for(int f:forms)if(f!=id&&BareFormName(f,base)==label)return label+" (Form "+std::to_string(n)+")";
        return label;
    }
    std::string BareFormName(int id,int base)const
    {
        const std::string name=Name(species,base,"SPECIES");
        const auto forms=engine.Forms(base);
        // (Kyogre's and Groudon's 100 stronger forms are their Primal Reversions.)
        const bool plus100=StatTotal(id)==StatTotal(base)+100;
        if(plus100&&(base==382||base==383))return "Primal "+name;
        // (Its Mega Stone: an item named for the species, its first four letters then "...ITE",
        // with an X, Y or Z after when it has more than one: Charizardite X, Mewtwonite Y.)
        auto upper=[](std::string t){for(char& ch:t)ch=(char)std::toupper((unsigned char)ch);return t;};
        const std::string stem=upper(name).substr(0,4);
        std::vector<std::string> stones;
        if(plus100)for(const auto& item:items)
        {
            const std::string u=upper(item);
            if(u.size()<=stem.size()+3||u.rfind(stem,0)!=0)continue;
            const size_t ite=u.rfind("ITE");
            if(ite==std::string::npos||ite<stem.size())continue;
            const std::string after=u.substr(ite+3);
            if(after.empty()||after==" X"||after==" Y"||after==" Z")stones.push_back(after);
        }
        // (A Mega: 100 stronger, and reverting after battle or with a stone of its own. The engine
        // marks the Megas as reverting since 2025; its earlier builds marked only the forms battles
        // change, as Castform's, so a stone named for the species tells them.)
        auto mega=[&](int f){return StatTotal(f)==StatTotal(base)+100&&(engine.formReverts.empty()||engine.Reverts(f)||!stones.empty());};
        if(plus100&&mega(id))
        {
            std::vector<int> megas;for(int f:forms)if(mega(f))megas.push_back(f);
            std::string suffix;
            if(megas.size()>1&&stones.size()==megas.size())
                for(size_t i=0;i<megas.size();i++)if(megas[i]==id)suffix=stones[i];
            return "Mega "+name+suffix;
        }
        return name+" (Form "+std::to_string(FormIndex(id,base))+")";
    }
    // (Its form number: the game's own, from its species' list of forms when the ROM has one, else
    // its place among them.)
    int FormIndex(int id,int base)const
    {
        if(engine.FormNumber(id))return engine.FormNumber(id);
        const auto forms=engine.Forms(base);
        for(size_t i=0;i<forms.size();i++)if(forms[i]==id)return (int)i+1;
        return 0;
    }
    // The Pokedex's own numbering: HG-Engine's later Pokemon by their national numbers, not their
    // places in the ROM (Victini 494, kept at 544); its forms none.
    int DexCount(int maxSpecies)const{return engine.on?engine.nationalCount:maxSpecies;}
    int DexSpecies(int number)const{return engine.on?engine.Internal(number):number;}
    int DexNumber(int id)const{return engine.on?engine.National(id):id;}
    bool InDex(int id,int maxSpecies)const{return engine.on?engine.National(id)>0:id>=1&&id<=maxSpecies;}
    std::string Move(int id)const{return id?Name(moves,id,"MOVE"):"NONE";}
    std::string ItemName(int id)const{return id?Name(items,id,"ITEM"):"NONE";}
    std::string PlaceName(int map)const
    {
        if(map>=0&&map<(int)mapPlaces.size())return Name(places,mapPlaces[map],"PLACE");
        return "MAP "+std::to_string(map);
    }
    struct Encounter {int species=0,low=0,high=0;std::string method;};
    std::vector<Encounter> Encounters(int map)const
    {
        std::vector<Encounter> out;
        if(map<0||map>=(int)mapRecords.size())return out;
        int set=mapRecords[map].encounter;if(set<0||set>=(int)wildRecords.size())return out;
        const auto& b=wildRecords[set];
        auto add=[&](int species,int low,int high,const std::string& method)
        {
            if(species<1||species>(engine.on?engine.count-1:gen5?649:493)||low<0||high<low||high>100)return;
            if((low==0)!=(high==0))return;
            for(auto& entry:out)if(entry.species==species&&entry.method==method)
            {
                if(entry.low==0||low==0){entry.low=entry.high=0;}
                else{entry.low=std::min(entry.low,low);entry.high=std::max(entry.high,high);}
                return;
            }
            out.push_back({species,low,high,method});
        };
        if(gen5)
        {
            if(b.size()!=232&&b.size()!=928)return out;
            static const int counts[]={12,12,12,5,5,5,5},offsets[]={8,56,104,152,172,192,212};
            static const char* methods[]={"GRASS","DARK GRASS","RUSTLING GRASS","SURF","RIPPLING WATER","FISHING","RIPPLING FISHING"};
            static const char* seasons[]={"SPRING ","SUMMER ","AUTUMN ","WINTER "};
            for(size_t block=0;block<b.size()/232;block++)for(int method=0;method<7;method++)if(b[block*232+method])
                for(int slot=0;slot<counts[method];slot++)
                {
                    size_t at=block*232+offsets[method]+slot*4;
                    add(U16(b.data()+at)&2047,b[at+2],b[at+3],std::string(b.size()==928?seasons[block]:"")+methods[method]);
                }
        }
        else if(hgss)
        {
            if(b.size()<196)return out;
            static const char* times[]={"MORNING GRASS","DAY GRASS","NIGHT GRASS"};
            if(b[0])for(int time=0;time<3;time++)for(int slot=0;slot<12;slot++)add(U16(b.data()+20+time*24+slot*2),b[8+slot],b[8+slot],times[time]);
            static const int counts[]={5,2,5,5,5};static const char* methods[]={"SURF","ROCK SMASH","OLD ROD","GOOD ROD","SUPER ROD"};
            size_t offset=100;
            for(int method=0;method<5;method++)
            {
                if(b[method+1])for(int slot=0;slot<counts[method];slot++)
                {size_t at=offset+slot*4;add(U16(b.data()+at+2),b[at],b[at+1],methods[method]);}
                offset+=counts[method]*4;
            }
            if(b[0])
            {
                for(int slot=0;slot<4;slot++)add(U16(b.data()+92+slot*2),0,0,slot<2?"HOENN RADIO":"SINNOH RADIO");
                for(int slot=0;slot<2;slot++)add(U16(b.data()+188+slot*2),0,0,"SWARM");
            }
            if(b[5])add(U16(b.data()+192),0,0,"NIGHT FISHING REPLACEMENT");
            if(b[3]||b[4]||b[5])add(U16(b.data()+194),0,0,"FISHING SWARM");
        }
        else
        {
            if(b.size()<424)return out;
            if(U32(b.data()))
            {
                for(int slot=0;slot<12;slot++)add(U32(b.data()+8+slot*8),U32(b.data()+4+slot*8),U32(b.data()+4+slot*8),"GRASS / CAVE");
                static const int slots[]={0,1,2,3,2,3,4,5,10,11,8,9,8,9,8,9,8,9,8,9};
                for(int n=0;n<20;n++)
                {int level=U32(b.data()+4+slots[n]*8);add(U32(b.data()+100+n*4+(n>=10?24:0)),level,level,n<2?"SWARM":n<6?"TIME REPLACEMENT":n<10?"POKE RADAR":"GBA SLOT");}
            }
            static const char* methods[]={"SURF","UNUSED","OLD ROD","GOOD ROD","SUPER ROD"};
            for(int method=0;method<5;method++)if(method!=1&&U32(b.data()+204+method*44))for(int slot=0;slot<5;slot++)
            {size_t at=208+method*44+slot*8;unsigned levels=U32(b.data()+at);add(U32(b.data()+at+4),(levels>>8)&255,levels&255,methods[method]);}
        }
        return out;
    }
    // Where to find a species: every place whose wild encounters have it, by how (merged over the
    // place's maps), worked out once from the encounter tables above.
    struct Habitat {std::string place,method;int low=0,high=0;};
    mutable std::vector<std::vector<Habitat>> habitats;
    mutable bool habitatsBuilt=false;
    const std::vector<Habitat>& WhereToFind(int id)const
    {
        static const std::vector<Habitat> none;
        if(!habitatsBuilt)
        {
            habitatsBuilt=true;habitats.assign(engine.on?engine.count:gen5?650:494,{});
            std::set<int> sets;
            for(int map=0;map<(int)mapRecords.size();map++)
            {
                const int set=mapRecords[map].encounter;if(set<0)continue;
                const std::string place=Name(places,mapRecords[map].place,"PLACE");
                if(!sets.insert(set*4096+std::max(0,mapRecords[map].place)).second)continue; // (the same table at the same place)
                for(const auto& e:Encounters(map))
                {
                    if(e.species<=0||e.species>=(int)habitats.size())continue;
                    bool merged=false;
                    for(auto& h:habitats[e.species])if(h.place==place&&h.method==e.method)
                    {
                        if(h.low==0||e.low==0)h.low=h.high=0;else{h.low=std::min(h.low,e.low);h.high=std::max(h.high,e.high);}
                        merged=true;break;
                    }
                    if(!merged)habitats[e.species].push_back({place,e.method,e.low,e.high});
                }
            }
            // (An HG-Engine hack's forms in the wild, as regional ones, on their species' page too.)
            if(engine.on)for(int f=engine.formStart;f<(int)habitats.size();f++)
            {
                const int base=engine.Base(f);
                if(base==f||habitats[f].empty())continue;
                const std::string tag=" (FORM "+std::to_string(FormIndex(f,base))+")";
                for(auto h:habitats[f]){h.method+=tag;habitats[base].push_back(h);}
            }
        }
        return id>0&&id<(int)habitats.size()?habitats[id]:none;
    }
    std::vector<std::string> EncounterLines(int map)const
    {
        std::vector<std::string> out;std::string method;
        for(const auto& entry:Encounters(map))
        {
            if(method!=entry.method){method=entry.method;out.push_back(method);}
            out.push_back(Species(entry.species)+(entry.low?" L"+std::to_string(entry.low)+(entry.high!=entry.low?"-"+std::to_string(entry.high):""):" - LEVEL VARIES"));
        }
        return out;
    }
    struct Marker {int x=0,y=0;bool ready=false,approximate=false;};
    Marker PlayerMarker(const Snapshot& snapshot)const
    {
        Marker out;
        if(!snapshot.ready||!regionMap.Ready())return out;
        if(gen5)
        {
            if(snapshot.map<0||snapshot.map>=(int)mapPlaces.size())return out;
            int place=mapPlaces[snapshot.map];
            if(place<0||place>=(int)placePoints.size()||placePoints[place].first<0)return out;
            out={placePoints[place].first,placePoints[place].second,true,true};
        }
        else
        {
            int map=snapshot.map,x=snapshot.x,y=snapshot.y;
            if(map<0||map>=(int)mapRecords.size())return out;
            const auto* record=&mapRecords[map];int bx=-1,by=-1;
            if(record->matrix==0&&x>=0&&y>=0){bx=x/32;by=y/32;}
            else if(hgss&&record->worldX>=0&&(record->worldX||record->worldY)){bx=record->worldX;by=record->worldY;out.approximate=true;}
            else if(snapshot.exitMap>=0&&snapshot.exitMap<(int)mapRecords.size()&&mapRecords[snapshot.exitMap].matrix==0&&snapshot.exitX>=0&&snapshot.exitY>=0)
            {bx=snapshot.exitX/32;by=snapshot.exitY/32;out.approximate=true;}
            if(bx<0||by<0||bx>=(hgss?47:30)||by>=(hgss?17:30))return {};
            out.x=hgss?bx*8+4:bx*7+5;out.y=hgss?(by+2)*8+4:by*7-34;out.ready=true;
        }
        if(out.x<0||out.y<0||out.x>=regionMap.width||out.y>=regionMap.height)return {};
        return out;
    }
    std::string ItemDescription(int id)const
    {
        return id>0&&id<(int)itemDescriptions.size()&&!itemDescriptions[id].empty()?itemDescriptions[id]:"NO DESCRIPTION AVAILABLE FOR THIS ITEM.";
    }

    int Type(int id,int which)const {if(id<0||id>=(int)personal.size()||personal[id].size()<8)return -1;int t=personal[id][6+which];return gen5 && t>=9?t+1:t;}
    struct Learned {int move=0,level=0;};
    std::vector<Learned> LearnedMoves(int id)const
    {
        std::vector<Learned> out;
        if(engine.on)
        {
            // (HG-Engine: a row for each species, {move, level} each until 0xFFFF. A form with
            // none of its own, as the Gigantamax ones, learns as its species.)
            const uint8_t* row=HgEngine::Row(engine.learnsets,engine.learnStride,id);
            // (A level past 100, as Koraidon's 981 in some builds' data, is never reached: left out.)
            for(size_t at=0;row&&at+4<=engine.learnStride;at+=4){int move=U16(row+at);if(move==65535)break;if(move&&U16(row+at+2)<=100)out.push_back({move,(int)U16(row+at+2)});}
            if(out.empty()&&engine.UsedForm(id))return LearnedMoves(engine.Base(id));
            return out;
        }
        if(id<0||id>=(int)learnsets.size())return out;auto& b=learnsets[id];
        for(size_t at=0;Range(b.size(),at,gen5?4:2);at+=gen5?4:2){int value=U16(b.data()+at);if(value==65535)break;out.push_back({gen5?value:value&511,gen5?(int)U16(b.data()+at+2):value>>9});}
        return out;
    }
    std::vector<Learned> Upcoming(const Mon& mon,size_t limit=3)const
    {
        std::vector<Learned> out;
        if(mon.egg)return out;
        auto learned=LearnedMoves(mon.species);
        std::stable_sort(learned.begin(),learned.end(),[](const Learned& a,const Learned& b){return a.level<b.level;});
        for(const auto& entry:learned)
        {
            if(entry.level<=mon.level||entry.level>100||entry.move<=0)continue;
            if(std::find(mon.moves.begin(),mon.moves.end(),entry.move)!=mon.moves.end())continue;
            if(std::any_of(out.begin(),out.end(),[&](const Learned& other){return other.move==entry.move;}))continue;
            if(out.size()>=limit)break;
            out.push_back(entry);
        }
        return out;
    }
    std::string Evolution(int method,int value)const
    {
        if(gen5 && method==7)return "TRADE WITH THE REQUIRED PARTNER";
        if(engine.on&&method>26)
        {
            // (HG-Engine's own methods, its armips/include/constants.s.)
            const std::string lv="LEVEL "+std::to_string(value);
            switch(method)
            {
            case 27:return lv+", DAYTIME";
            case 28:return lv+", NIGHTTIME";
            case 29:return lv+", AT DUSK";
            case 30:return lv+", WHILE IT RAINS";
            case 31:return std::string("LEVEL UP KNOWING A ")+CompanionTypes::Name(value)+"-TYPE MOVE";
            case 32:return lv+", A DARK-TYPE POKEMON IN THE PARTY";
            case 33:return "TRADE FOR "+Species(value);
            case 34:return lv+", AN AMPED NATURE";
            case 35:return lv+", A LOW KEY NATURE";
            case 36:return "LAND "+std::to_string(value)+" CRITICAL HITS IN ONE BATTLE";
            case 37:return "LOSE "+std::to_string(value)+" HP IN BATTLE, THEN GO UNDER THE STONE ARCH";
            case 47:return lv+", A PARTICULAR FORM";
            case 48:return "WALK "+std::to_string(value?value:1000)+" STEPS WITH IT OUT";
            default:if(method>=38&&method<=46)return "SPIN AROUND WHILE IT HOLDS A SWEET";
                return "SPECIAL EVOLUTION CONDITION";
            }
        }
        if(gen5 && method>7)--method;
        const std::string level="LEVEL "+std::to_string(value);
        switch(method)
        {
        case 1:return "LEVEL UP WITH HIGH FRIENDSHIP";
        case 2:return "HIGH FRIENDSHIP, DAYTIME";
        case 3:return "HIGH FRIENDSHIP, NIGHTTIME";
        case 4:case 13:return level;
        case 5:return "TRADE";
        case 6:return "TRADE HOLDING "+ItemName(value);
        case 7:return "USE "+ItemName(value);
        case 8:return level+", ATTACK ABOVE DEFENCE";
        case 9:return level+", ATTACK EQUAL TO DEFENCE";
        case 10:return level+", ATTACK BELOW DEFENCE";
        case 11:case 12:return level+", PERSONALITY BRANCH";
        case 14:return level+", SPARE PARTY SLOT AND BALL";
        case 15:return "LEVEL UP WITH HIGH BEAUTY";
        case 16:return "MALE: USE "+ItemName(value);
        case 17:return "FEMALE: USE "+ItemName(value);
        case 18:return "LEVEL UP HOLDING "+ItemName(value)+", DAYTIME";
        case 19:return "LEVEL UP HOLDING "+ItemName(value)+", NIGHTTIME";
        case 20:return "LEVEL UP KNOWING "+Move(value);
        case 21:return "LEVEL UP WITH "+Species(value)+" IN PARTY";
        case 22:return level+", MALE";
        case 23:return level+", FEMALE";
        case 24:return "LEVEL UP IN A MAGNETIC AREA";
        case 25:return "LEVEL UP NEAR A MOSS ROCK";
        case 26:return "LEVEL UP NEAR AN ICE ROCK";
        default:return "SPECIAL EVOLUTION CONDITION";
        }
    }
    struct MoveInfo {bool ready=false;int type=-1,category=0,power=0,accuracy=0,pp=0;};
    MoveInfo MoveData(int id)const
    {
        if(id<=0||id>=(int)moveRecords.size())return {};
        const auto& b=moveRecords[id];if(b.size()<(gen5?6u:7u))return {};
        MoveInfo out;out.type=b[gen5?0:4];if(gen5&&out.type>=9)out.type++;
        out.category=b[2];if(out.category>2||out.type>17||(out.type==9&&!CompanionTypes::fairy))return {};
        out.power=b[3];out.accuracy=b[gen5?4:5];out.pp=b[gen5?5:6];out.ready=true;return out;
    }
    struct TrainerMon {int species=0,level=0,item=0,form=0,ability=0,nature=-1;std::array<int,4> moves{};bool customMoves=false;};
    // HG-Engine's trainers (its tools/source/trainerdatagen): the header's first byte flags what
    // each Pokemon has after its IVs, ability slot, level and species (the form in the top 5 bits):
    // 0x02 an item, 0x01 moves, 0x04 an ability, 0x08 a ball, 0x10 IVs and EVs, 0x20 a nature,
    // 0x40 a shiny lock, 0x80 more (a word of flags, then what they say); then a ball seal.
    // (Which extra fields its trainers' Pokemon have: 0 the engine's own since 2026 (the PP counts
    // 0x80, a nickname 0x100); 1 its earlier builds' (their types 0x80, then the PP counts 0x100 and
    // the nickname 0x200: armips/include/macros.s, src/field/enemy_party.c, 2024-2025).)
    int trainerLayout=0;
    std::vector<TrainerMon> EngineTeam(const Bytes& header,const Bytes& team,int layout=-1)const
    {
        std::vector<TrainerMon> out;
        if(header.size()<4)return out;
        if(layout<0)layout=trainerLayout;
        const int flags=header[0],count=header[3]&0x7F;
        if(count<1||count>6)return out;
        size_t at=0;bool oddLevel=false;
        for(int slot=0;slot<count;slot++)
        {
            if(!Range(team.size(),at,6))return {};
            TrainerMon mon;
            mon.level=U16(team.data()+at+2);const int sp=U16(team.data()+at+4);at+=6;
            mon.species=sp&0x7FF;mon.form=sp>>11;
            if(mon.species<1||mon.species>=engine.count)return {};
            // (A level no Pokemon has (Mirror Gold's own level scaling sets a few): kept as not known,
            // 0, when every byte of the team is then read exactly.)
            if(mon.level<1||mon.level>100){mon.level=0;oddLevel=true;}
            mon.species=engine.FormSpecies(mon.species,mon.form); // (a form: its own record, its own moves)
            auto take=[&](size_t n){const size_t here=at;at+=n;return Range(team.size(),here,n)?team.data()+here:nullptr;};
            if(flags&0x02){auto b=take(2);if(!b)return {};mon.item=U16(b);}
            mon.customMoves=flags&0x01;
            if(flags&0x01){auto b=take(8);if(!b)return {};for(int n=0;n<4;n++)mon.moves[n]=U16(b+n*2);}
            if(flags&0x04){auto b=take(2);if(!b)return {};mon.ability=U16(b);}
            if(flags&0x08)take(2);
            if(flags&0x10)take(12);
            if(flags&0x20){auto b=take(1);if(!b)return {};mon.nature=*b;}
            if(flags&0x40)take(1);
            if(flags&0x80)
            {
                auto b=take(4);if(!b)return {};
                const uint32_t more=U32(b);
                static const std::vector<std::pair<uint32_t,size_t>> extra[2]={
                    {{0x01,4},{0x02,2},{0x04,2},{0x08,2},{0x10,2},{0x20,2},{0x40,2},{0x80,4},{0x100,22}},
                    {{0x01,4},{0x02,2},{0x04,2},{0x08,2},{0x10,2},{0x20,2},{0x40,2},{0x80,2},{0x100,4},{0x200,22}}};
                for(const auto& e:extra[layout?1:0])if(more&e.first)take(e.second);
            }
            take(2); // (the ball seal)
            if(!mon.customMoves)
            {
                std::vector<int> learned;
                for(const auto& entry:LearnedMoves(mon.species))if(entry.level<=mon.level&&entry.move>0&&std::find(learned.begin(),learned.end(),entry.move)==learned.end())
                {if(learned.size()==4)learned.erase(learned.begin());learned.push_back(entry.move);}
                for(size_t n=0;n<learned.size();n++)mon.moves[n]=learned[n];
            }
            out.push_back(mon);
        }
        if(oddLevel&&at!=team.size())return {};
        return Range(team.size(),0,at)?out:std::vector<TrainerMon>{};
    }
    std::vector<TrainerMon> TrainerTeam(int id)const
    {
        std::vector<TrainerMon> out;
        if(id<=0||id>=(int)trainerData.size()||id>=(int)trainerTeams.size())return out;
        const auto& header=trainerData[id];const auto& team=trainerTeams[id];
        if(engine.on)return EngineTeam(header,team);
        if(header.size()<4||header[0]>3||header[3]<1||header[3]>6)return out;
        bool moves=header[0]&1,items=header[0]&2;
        size_t stride=8+(moves?8:0)+(items?2:0);
        if(!Range(team.size(),0,stride*header[3]))return out;
        for(int slot=0;slot<header[3];slot++)
        {
            size_t at=slot*stride;TrainerMon mon;
            mon.level=U16(team.data()+at+2);mon.species=U16(team.data()+at+4);
            mon.form=gen5?U16(team.data()+at+6):mon.species>>10;
            if(!gen5)mon.species&=511;
            if(mon.level<1||mon.level>100||mon.species<1||mon.species>(gen5?649:493))return {};
            at+=gen5?8:6;
            if(items){mon.item=U16(team.data()+at);at+=2;}
            mon.customMoves=moves;
            if(moves)for(int n=0;n<4;n++)mon.moves[n]=U16(team.data()+at+n*2);
            else
            {
                std::vector<int> learned;
                for(const auto& entry:LearnedMoves(mon.species))if(entry.level<=mon.level&&entry.move>0&&std::find(learned.begin(),learned.end(),entry.move)==learned.end())
                {if(learned.size()==4)learned.erase(learned.begin());learned.push_back(entry.move);}
                for(size_t n=0;n<learned.size();n++)mon.moves[n]=learned[n];
            }
            out.push_back(mon);
        }
        return out;
    }
    // The trainers that matter most: gym leaders, the Elite Four and Champion, the rival, Team
    // Rocket's leaders and other named PKMN Trainers (by their class's name, from the ROM).
    std::vector<int> keyTrainers;
    std::string TrainerLevels(int id)const
    {
        int low=0,high=0;
        for(const auto& mon:TrainerTeam(id))if(mon.level>0){low=low?std::min(low,mon.level):mon.level;high=std::max(high,mon.level);}
        if(!low)return "";
        return "LV "+std::to_string(low)+(high!=low?"-"+std::to_string(high):"");
    }
    // (A key trainer's team by its own name, as GUIDE's GYMS and NEXT BOSS name them ("FALKNER",
    // "LT_SURGE"): the first trainer of that name after its class with a team, a hack's own.)
    std::vector<TrainerMon> KeyTeam(const std::string& who)const
    {
        auto words=[](const std::string& t)
        {
            std::vector<std::string> out;std::string w;
            for(char ch:t+" ")
            {
                if(ch==' '||ch=='_'){if(!w.empty())out.push_back(w);w.clear();}
                else if(std::isalnum((unsigned char)ch))w+=(char)std::toupper((unsigned char)ch);
            }
            return out;
        };
        const auto want=words(who);
        if(want.empty())return {};
        for(int id:keyTrainers)
        {
            const auto have=words(Name(trainerNames,id,"TRAINER"));
            if(have.size()>want.size()&&std::equal(want.begin(),want.end(),have.end()-(std::ptrdiff_t)want.size()))return TrainerTeam(id);
        }
        return {};
    }
    static bool ReferenceMatches(std::string name,const std::string& query)
    {
        for(char& c:name)c=(char)std::toupper((unsigned char)c);
        std::istringstream words(query);std::string word;
        while(words>>word)
        {
            for(char& c:word)c=(char)std::toupper((unsigned char)c);
            if(name.find(word)==std::string::npos)return false;
        }
        return true;
    }
    std::vector<std::pair<int,std::string>> ReferenceEntries(int kind,const std::string& query)const
    {
        const auto& names=kind==0?species:kind==1?items:kind==2?moves:kind==3?places:trainerNames;
        std::vector<std::pair<int,std::string>> out;
        if(kind<0||kind>4)return out;
        if(kind==0&&engine.on)
        {
            for(int n=1;n<=engine.nationalCount;n++){const int id=engine.Internal(n);const std::string name=Species(id);if(ReferenceMatches(name,query))out.push_back({id,name});}
            for(int id=engine.formStart;id<engine.count;id++){if(!engine.UsedForm(id))continue;const std::string name=Species(id);if(ReferenceMatches(name,query))out.push_back({id,name});}
            return out;
        }
        for(size_t id=kind==3?0:1;id<names.size();id++)
            if(!names[id].empty()&&ReferenceMatches(names[id],query))out.push_back({(int)id,names[id]});
        return out;
    }
    std::vector<std::string> AbilityDetails(int id)const
    {
        std::vector<std::string> out;
        if(id<=0||id>=(int)personal.size())return out;
        if(engine.on)
        {
            // (HG-Engine: two-byte abilities at 0x16 and 0x1A (HeartGold's bytes at 0x16 and 0x17 in
            // its earlier builds); the hidden one in its own table.)
            const auto& p=personal[id];if(p.size()<0x1C)return out;
            const int hidden=id<(int)engine.hidden.size()?engine.hidden[id]:0;
            const int first=engine.wideAbilities?U16(p.data()+0x16):p[0x16],second=engine.wideAbilities?U16(p.data()+0x1A):p[0x17];
            for(int a:{first,second,hidden})if(a>0&&a<(int)abilities.size())
            {
                std::string line=(a==hidden&&a!=first&&a!=second?"HIDDEN ABILITY: ":"ABILITY: ")+Name(abilities,a,"ABILITY");
                if(std::find(out.begin(),out.end(),line)==out.end())out.push_back(line);
            }
            return out;
        }
        const auto& p=personal[id];size_t at=gen5?24:22,count=gen5?3:2;
        if(!Range(p.size(),at,count))return out;
        for(size_t i=0;i<count;i++)if(p[at+i])
        {
            std::string line=(gen5&&i==2?"HIDDEN ABILITY: ":"ABILITY: ")+Name(abilities,p[at+i],"ABILITY");
            if(std::find(out.begin(),out.end(),line)==out.end())out.push_back(line);
        }
        return out;
    }
    // (An evolution record's Pokemon. HG-Engine keeps a form in the target's top five bits (Pikachu's
    // to Alolan Raichu): the form's own record where the engine has one, else the species and its
    // form's number; and its EVO_DUMMY (49) records are none in play.)
    int EvoTarget(int raw)const
    {
        if(!engine.on)return raw;
        const int species=raw&0x7FF,form=raw>>11;
        return form?engine.FormSpecies(species,form):species;
    }
    std::string EvoName(int raw)const
    {
        const int target=EvoTarget(raw);
        if(engine.on&&(raw>>11)&&target==(raw&0x7FF))return Species(target)+" (FORM "+std::to_string(raw>>11)+")";
        return Species(target);
    }
    bool EvoNone(int method)const{return !method||(engine.on&&method==49);}
    std::vector<std::string> EvolutionDetails(int id)const
    {
        std::vector<std::string> out;
        if(id<=0||id>=(int)evolutions.size())return out;
        const auto& records=evolutions[id];
        for(size_t at=0;Range(records.size(),at,6);at+=6)
        {
            const int method=U16(records.data()+at),value=U16(records.data()+at+2),raw=U16(records.data()+at+4);
            if(!EvoNone(method)&&EvoTarget(raw)>0)out.push_back("EVOLVES TO "+EvoName(raw)+": "+Evolution(method,value));
        }
        return out;
    }
    // (The whole family for the POKEDEX: find the root by following the first parent, then walk
    // every branch forward. Edges touching the current species are marked, so the entry's place
    // in its family is clear.)
    std::vector<std::string> EvolutionFamily(int id)const
    {
        std::vector<std::string> out;
        if(id<=0||id>=(int)evolutions.size())return out;
        std::vector<int> parent(evolutions.size(),-1);
        for(int s=1;s<(int)evolutions.size();s++)
            for(size_t at=0;at+5<evolutions[s].size();at+=6)
            {
                if(EvoNone(U16(evolutions[s].data()+at)))continue;
                const int target=EvoTarget(U16(evolutions[s].data()+at+4));
                if(target>0&&target<(int)parent.size()&&parent[target]<0)parent[target]=s;
            }
        int root=id;
        std::set<int> guard;
        while(root>0&&root<(int)parent.size()&&!guard.count(root))
        {
            guard.insert(root);
            const int p=parent[root];
            if(p<=0)break;
            root=p;
        }
        std::vector<int> stack{root};
        std::set<int> seen;
        while(!stack.empty())
        {
            const int s=stack.back();stack.pop_back();
            if(s<=0||s>=(int)evolutions.size()||!seen.insert(s).second)continue;
            const auto& records=evolutions[s];
            for(size_t at=0;Range(records.size(),at,6);at+=6)
            {
                const int method=U16(records.data()+at),value=U16(records.data()+at+2),raw=U16(records.data()+at+4),target=EvoTarget(raw);
                if(EvoNone(method)||target<=0||target>=(int)evolutions.size())continue;
                std::string line=Species(s)+" -> "+EvoName(raw)+": "+Evolution(method,value);
                if(s==id||target==id)line+="  (THIS ONE)";
                out.push_back(line);
                stack.push_back(target);
            }
        }
        return out;
    }
    // (What is ahead from here for the PARTY: the current species and everything it can become,
    // never its earlier forms.)
    std::vector<std::string> EvolutionAhead(int id)const
    {
        std::vector<std::string> out;
        if(id<=0||id>=(int)evolutions.size())return out;
        std::vector<int> stack{id};
        std::set<int> seen;
        while(!stack.empty())
        {
            const int s=stack.back();stack.pop_back();
            if(s<=0||s>=(int)evolutions.size()||!seen.insert(s).second)continue;
            const auto& records=evolutions[s];
            for(size_t at=0;Range(records.size(),at,6);at+=6)
            {
                const int method=U16(records.data()+at),value=U16(records.data()+at+2),raw=U16(records.data()+at+4),target=EvoTarget(raw);
                if(EvoNone(method)||target<=0||target>=(int)evolutions.size())continue;
                out.push_back(Species(s)+" -> "+EvoName(raw)+": "+Evolution(method,value));
                stack.push_back(target);
            }
        }
        return out;
    }
    void Load(const Bytes& rom,const std::string& code,const Profile& profile)
    {
        // (The type chart a game before this one set, a hack's own or a GBA hack's: this game's own from here.)
        CompanionTypes::fairy=false;CompanionTypes::romChart.clear();
        if(!profile.supported)return;
        gen5=profile.gen5;
        const std::string family=code.substr(0,std::min<size_t>(3,code.size()));
        bool hg=family=="IPK"||family=="IPG";hgss=hg;
        const bool ss=family=="IPG";
        // (Black 2 and White 2: UPR ZX's gen5_offsets.ini text banks and archives, each bank checked
        // by what it holds and else looked for.)
        const bool b2=family=="IRE"||family=="IRD";
        auto msg=Narc(RomFile(rom,gen5?"a/0/0/2":hg?"a/0/2/7":"msgdata/pl_msg.narc"));
        auto bank=[&](int n){return n>=0&&n<(int)msg.size()?Text(msg[n],gen5):std::vector<std::string>{};};
        auto upper=[](std::string t){for(char& ch:t)ch=(char)std::toupper((unsigned char)ch);return t;};
        auto checked=[&](int n,int entry,const char* want)
        {
            auto b=bank(n);
            if(entry<(int)b.size()&&upper(b[entry]).find(want)!=std::string::npos)return b;
            for(size_t i=0;i<msg.size();i++){b=bank((int)i);if(entry<(int)b.size()&&b.size()>100&&upper(b[entry])==want)return b;}
            return std::vector<std::string>{};
        };
        if(b2)
        {
            species=checked(90,1,"BULBASAUR");moves=checked(403,1,"POUND");items=checked(64,1,"MASTER BALL");
            places=bank(109);
        }
        else {species=bank(gen5?70:hg?237:412);moves=bank(gen5?203:hg?750:647);items=bank(gen5?54:hg?222:392);
        // HGSS item.c LoadItemDescription: msg_0221; Platinum TEXT_BANK_ITEM_DESCRIPTIONS: 391.
        // Black (U) text bank 53 is also identified by UPR ZX's gen5_offsets.ini.
        places=bank(gen5?89:hg?279:433);}
        // (A hack whose text is laid out otherwise, as Origin HeartGold's (the Japanese game's, its
        // banks moved): nothing read from it, rather than other text in place of its names.)
        if(!gen5&&!b2&&(species.size()<494||moves.size()<400||items.size()<400))
        {
            unreadable=true;species.clear();moves.clear();items.clear();places.clear();
            return;
        }
        regionMap=ReferenceMap(Narc(RomFile(rom,gen5?"a/0/8/5":hg?"a/1/4/4":"graphic/tmap_gra.narc")),gen5,hg);
        if(!gen5){monIcons=Narc(RomFile(rom,hg?"a/0/2/0":"poketool/icongra/pl_poke_icon.narc"));if(!monIcons.empty())iconPalettes=CompanionIcons::Palettes(monIcons[0]);}
        else
        {
            monIcons=Narc(RomFile(rom,"a/0/0/7"));
            const Bytes arm=RomArm9(rom);
            if(!monIcons.empty()){iconPalettes=CompanionIcons::Palettes(monIcons[0]);FindGen5IconPalettes(arm);}
            for(size_t i=0;i<monIcons.size()&&i<16;i++)if(monIcons[i].size()>4&&!memcmp(monIcons[i].data(),"RGCN",4)){gen5IconStart=(int)i;break;}
            LoadItemIcons(rom,arm,false);
        }
        if(gen5)
        {
            auto zones=Narc(RomFile(rom,"a/0/1/2"));
            if(!zones.empty()&&zones[0].size()%48==0&&zones[0].size()/48<=4096)
                for(size_t at=0;Range(zones[0].size(),at,48);at+=48)
                {
                    // (Black 2 and White 2 keep flags in the top three bits; 0x1FFF there: none.)
                    MapRecord record;record.place=zones[0][at+0x1A];int set=U16(zones[0].data()+at+0x14);
                    if(b2)set=(set&0x1FFF)==0x1FFF?65535:set&0x1FFF;
                    record.encounter=set==65535?-1:set;
                    mapPlaces.push_back(record.place);mapRecords.push_back(record);
                }
        }
        const size_t wildRecordCount=Narc(RomFile(rom,b2?"a/1/2/7":gen5?"a/1/2/6":hg?(ss?"a/1/3/6":"a/0/3/7"):"fielddata/encountdata/pl_enc_data.narc")).size();
        if(!gen5)
        {
            auto arm=RomArm9(rom),names=RomFile(rom,"fielddata/maptable/mapname.bin");
            LoadItemIcons(rom,arm,hg);
            size_t offset=hg?0xF6BE0:0xE601C,count=names.size()/16;
            // (Another revision: the headers looked for, each place a name and each encounter table
            // one of the ROM's or none.)
            auto fits=[&](size_t at)
            {
                if(!Range(arm.size(),at,count*24))return false;
                for(size_t n=0;n<count;n++)
                {
                    const auto* h=arm.data()+at+n*24;
                    if(h[18]>=places.size())return false;
                    const int e=hg?(h[0]==255?-1:h[0]):(U16(h+14)==65535?-1:U16(h+14));
                    if(e>=(int)wildRecordCount)return false;
                }
                return true;
            };
            // (A hack with DSPRE's "dynamic headers" (Heartfelt's): each header its own member of a/0/5/0,
            // as many as the map names; used first when they all fit, as the ARM9 then keeps old ones.)
            Bytes dynamic;
            if(hg)
            {
                auto headers=Narc(RomFile(rom,"a/0/5/0"));
                bool all=count>0&&headers.size()==count;
                for(size_t n=0;n<headers.size()&&all;n++)all=headers[n].size()==24;
                if(all){for(const auto& h:headers)dynamic.insert(dynamic.end(),h.begin(),h.end());}
                if(all){arm.swap(dynamic);offset=0;if(!fits(0)){arm.swap(dynamic);dynamic.clear();}}
            }
            if(dynamic.empty()&&count>0&&!fits(offset))for(size_t at=0;at+count*24<=arm.size();at+=4)if(fits(at)){offset=at;break;}
            // UPR ZX gen4_offsets.ini/loadWildMapNames: fixed 24-byte headers, place at +18.
            if(names.size()%16==0&&count>0&&count<=4096&&Range(arm.size(),offset,count*24))
            {
                std::vector<MapRecord> records;std::vector<int> mapped;
                for(size_t n=0;n<count;n++)
                {
                    const auto* h=arm.data()+offset+n*24;MapRecord record;record.place=h[18];
                    if(record.place>=(int)places.size()){records.clear();break;}
                    record.matrix=U16(h+(hg?4:2));record.encounter=hg?(h[0]==255?-1:h[0]):(U16(h+14)==65535?-1:U16(h+14));
                    if(hg){unsigned coords=U16(h+2);record.worldX=(coords>>4)&63;record.worldY=coords>>10;record.scripts=U16(h+6);record.events=U16(h+16);}
                    records.push_back(record);mapped.push_back(record.place);
                }
                if(records.size()==count){mapRecords=std::move(records);mapPlaces=std::move(mapped);}
            }
        }
        else
        {
            placePoints.assign(places.size(),{-1,-1});auto towns=Narc(RomFile(rom,"a/0/8/6"));
            if(!towns.empty()&&towns[0].size()%54==0&&towns[0].size()/54<=4096)
                for(size_t at=0;Range(towns[0].size(),at,54);at+=54)
                {
                    const auto* t=towns[0].data()+at;int map=U16(t),x=U16(t+4),y=U16(t+6);
                    if(map<(int)mapPlaces.size()&&mapPlaces[map]<(int)placePoints.size()&&x<regionMap.width&&y<regionMap.height)placePoints[mapPlaces[map]]={x,y};
                }
        }
        wildRecords=Narc(RomFile(rom,b2?"a/1/2/7":gen5?"a/1/2/6":hg?(ss?"a/1/3/6":"a/0/3/7"):"fielddata/encountdata/pl_enc_data.narc"));
        itemDescriptions=bank(b2?63:gen5?53:hg?221:391);
        descriptions=bank(b2?442:gen5?235:hg?(ss?804:803):706);abilities=b2?checked(374,1,"STENCH"):bank(gen5?182:hg?720:610);
        // Trainer layouts and name banks are documented by UPR ZX Gen4/Gen5RomHandler.
        trainerData=Narc(RomFile(rom,b2?"a/0/9/1":gen5?"a/0/9/2":hg?"a/0/5/5":"poketool/trainer/trdata.narc"));
        trainerTeams=Narc(RomFile(rom,b2?"a/0/9/2":gen5?"a/0/9/3":hg?"a/0/5/6":"poketool/trainer/trpoke.narc"));
        trainerNames=bank(b2?382:gen5?190:hg?729:618);
        const auto trainerClasses=bank(b2?383:gen5?191:hg?730:619);
        personal=Narc(RomFile(rom,gen5?"a/0/1/6":hg?"a/0/0/2":"poketool/personal/pl_personal.narc"));
        evolutions=Narc(RomFile(rom,gen5?"a/0/1/9":hg?"a/0/3/4":"poketool/personal/evo.narc"));
        learnsets=Narc(RomFile(rom,gen5?"a/0/1/8":hg?"a/0/3/3":"poketool/personal/wotbl.narc"));
        moveRecords=Narc(RomFile(rom,gen5?"a/0/2/1":hg?"a/0/1/1":"poketool/waza/pl_waza_tbl.narc"));
        LoadDex(rom,hg,b2);
        // (An HG-Engine hack: its own tables in place of HeartGold's.)
        if(hg&&engine.Load(rom,personal,learnsets,species,std::max(moves.size(),moveRecords.size()),items,abilities.size(),msg))
        {
            if(!engine.machines.empty())machineMoves=engine.machines;
            // (Its item icons: each item's at 2 + 2 x its number, its palette next (hg-engine
            // src/item.c GetItemNarcData), not HeartGold's table.)
            if(itemIcons.size()>=items.size()*2+2&&itemIcons[4].size()>4&&!memcmp(itemIcons[4].data(),"RGCN",4)&&itemIcons[5].size()>4&&!memcmp(itemIcons[5].data(),"RLCN",4))
            {itemIconIds.clear();itemDataIds.clear();for(size_t i=0;i<items.size();i++)itemIconIds.push_back({(int)i*2+2,(int)i*2+3});}
            // (Its item descriptions: a text bank for each generation's items, 830 then every
            // fourth to 850 (hg-engine include/constants/file.h), together one for each item.)
            {
                std::vector<std::string> all;
                for(int b=830;b<=850;b+=4){auto t=bank(b);all.insert(all.end(),t.begin(),t.end());}
                // (A hack's own items after the engine's (Hardlove's Starter Card) have none there.)
                if(!all.empty()&&all.size()<=items.size()&&all.size()*10>=items.size()*9)itemDescriptions=std::move(all);
            }
            if(engine.eggStride)eggMoves.clear();
            if(!engine.tutorBits.empty()){tutorMoves.clear();tutorBits.clear();}
            for(size_t row=0;engine.eggStride&&Range(engine.eggs.size(),row*engine.eggStride,engine.eggStride);row++)
            {
                std::vector<uint16_t> list;
                for(size_t at=0;at+2<=engine.eggStride;at+=2){const unsigned m=U16(engine.eggs.data()+row*engine.eggStride+at);if(m==0xFFFF)break;if(m&&m<moveRecords.size())list.push_back((uint16_t)m);}
                eggMoves.push_back(list);
            }
        }
        // (Its trainers' extra fields: the layout that reads more of the teams that have them.)
        if(engine.on)
        {
            int read[2]={0,0};
            for(size_t id=1;id<trainerData.size()&&id<trainerTeams.size();id++)
                if(trainerData[id].size()>=4&&(trainerData[id][0]&0x80))for(int l=0;l<2;l++)read[l]+=!EngineTeam(trainerData[id],trainerTeams[id],l).empty();
            trainerLayout=read[1]>read[0]?1:0;
        }
        // (Each trainer's class before its name; HG-Engine's class is two bytes.)
        for(size_t id=1;id<trainerNames.size();id++)if(id<trainerData.size()&&trainerData[id].size()>=(engine.on?3u:2u))
        {
            const int cls=engine.on?U16(trainerData[id].data()+1):trainerData[id][1];
            if(cls>=(int)trainerClasses.size())continue;
            trainerNames[id]=trainerClasses[cls]+" "+trainerNames[id];
            std::string key=trainerClasses[cls];for(char& ch:key)ch=(char)std::toupper((unsigned char)ch);
            bool important=false;
            for(const char* k:{"LEADER","ELITE FOUR","CHAMPION","RIVAL","BOSS","EXECUTIVE","PKMN TRAINER","POKEMON TRAINER"})important=important||key.find(k)!=std::string::npos;
            if(important&&!TrainerTeam((int)id).empty())keyTrainers.push_back((int)id);
        }
        if(hg)
        {
            // Each item's pocket (its data's bits 7-10, 7 the key items) and where each item is found:
            // given, item balls and hidden items, from each map's scripts and events.
            const auto data=Narc(RomFile(rom,"a/0/1/7"));
            // (Item i's data: its file in the ARM9's table where there is one (HeartGold's, and the
            // engine's earlier builds'), else file i (the engine's own, a file for each item).)
            for(size_t id=0;id<items.size();id++)
            {
                const size_t f=itemDataIds.empty()?id:id<itemDataIds.size()?(size_t)itemDataIds[id]:data.size();
                itemPockets.push_back(f<data.size()&&data[f].size()>=10?(U16(data[f].data()+8)>>7)&15:-1);
            }
            std::vector<ItemSpotMap> maps;
            for(const auto& r:mapRecords)maps.push_back({r.place,r.scripts,r.events});
            if(!maps.empty()&&!items.empty())
                itemSpots=FindItemSpots(maps,Narc(RomFile(rom,"a/0/1/2")),Narc(RomFile(rom,"a/0/3/2")),Narc(RomFile(rom,"a/0/2/8")),items.size(),RomArm9(rom));
        }
    }
    // GUIDE's pages. An HG-Engine hack's own HMs and key items, where its scripts and maps put them,
    // in WHERE IS (before the hand-written ones left there); the hand-written pages that are not
    // about one of those items around them.
    mutable std::deque<std::string> guideText; // (the generated pages' text, kept for as long as the pages)
    std::vector<GuideEntry> Guide(const std::string& code)const
    {
        auto pages=GuideFor(code);
        if(!engine.on||itemSpots.empty())return pages;
        auto keep=[&](std::string t)->const char*{guideText.push_back(std::move(t));return guideText.back().c_str();};
        auto bare=[](std::string t){std::string o;for(char ch:t)if(std::isalnum((unsigned char)ch))o+=(char)std::toupper((unsigned char)ch);return o;};
        std::vector<GuideEntry> made;std::vector<std::string> covered,coveredAt;
        for(int pass=0;pass<2;pass++)
            for(int item=1;item<(int)itemSpots.size();item++)
            {
                const std::string name=ItemName(item);
                const bool hm=name.size()>2&&name.rfind("HM",0)==0&&std::isdigit((unsigned char)name[2]);
                const bool key=item<(int)itemPockets.size()&&itemPockets[item]==7;
                if((pass==0)!=hm||(!hm&&!key)||itemSpots[item].empty())continue;
                std::string title=name,answer;
                if(hm)
                {
                    // (Its move: from the engine's own list, or (its earlier builds) HeartGold's, HM01 item 420.)
                    int machine=-1;
                    for(int i=0;i<(int)engine.machineItems.size();i++)if(engine.machineItems[i]==item){machine=i;break;}
                    if(machine<0&&engine.machines.empty()&&item>=328&&item<=427)machine=item-328;
                    if(machine>=0&&machine<(int)machineMoves.size())title+=" "+Move(machineMoves[machine]);
                }
                if(std::find(covered.begin(),covered.end(),bare(name))!=covered.end())continue; // (two items of one name)
                for(const auto& spot:itemSpots[item])
                {
                    const std::string place=Name(places,spot.place,"PLACE");
                    answer+=(answer.empty()?"":" ")+std::string(spot.how==1?"An item ball in ":spot.how==2?"Hidden in ":"Given in ")+place+".";
                }
                const std::string where=Name(places,itemSpots[item][0].place,"PLACE");
                made.push_back({keep(title),keep(where+"."),keep(answer),"WHERE IS",item});
                covered.push_back(bare(name));coveredAt.push_back(bare(where));
            }
        // (A hand-written page about one of those items is left out, even under its old name: the
        // same first word and place (DOWSING MCHN, RAINBOW WING), or the BICYCLE.)
        auto firstWord=[&](const std::string& t){std::string w;for(char ch:t){if(ch==' ')break;w+=(char)std::toupper((unsigned char)ch);}return w;};
        std::vector<GuideEntry> out;bool placed=false;
        for(GuideEntry page:pages)
        {
            const std::string t=bare(page.title),at=bare(page.hint),w=firstWord(page.title);
            bool dup=false;
            for(size_t i=0;i<covered.size()&&!dup;i++)
            {
                const std::string& c=covered[i];
                dup=t==c||(t.size()>c.size()&&t.rfind(c,0)==0&&c.rfind("HM",0)==0)||(t=="BICYCLE"&&c=="BIKE");
                dup=dup||(w.size()>=4&&at==coveredAt[i]&&bare(w)==c.substr(0,std::min(c.size(),bare(w).size())));
            }
            if(dup)continue;
            // (Its item by HeartGold's number, for the row's HAVE: kept where the hack's item of that
            // number has the page's name.)
            if(page.item>0){const std::string n=bare(ItemName(page.item));if(n.empty()||t.rfind(n,0)!=0)page.item=0;}
            if(!placed&&page.section&&!std::strcmp(page.section,"WHERE IS")){out.insert(out.end(),made.begin(),made.end());placed=true;}
            out.push_back(page);
        }
        if(!placed)out.insert(out.begin(),made.begin(),made.end());
        return out;
    }
    void LoadDex(const Bytes& rom,bool hg,bool b2)
    {
        machineMoves=FindMachineMoves(RomArm9(rom),gen5);
        if(gen5)
        {
            auto eggs=Narc(RomFile(rom,b2?"a/1/2/4":"a/1/2/3"));
            for(auto& m:eggs)eggMoves.push_back(Gen5EggMoves(m));
        }
        else if(hg)
        {
            auto eggs=Narc(RomFile(rom,"a/2/2/9"));
            if(!eggs.empty())eggMoves=Gen4EggMoves(eggs[0],0);
        }
        else
        {
            auto overlay=RomOverlay(rom,5);size_t at=FindGen4EggList(overlay);
            if(at!=std::string::npos)eggMoves=Gen4EggMoves(overlay,at);
        }
        // (Move tutors: Platinum's in overlay 5, 12 bytes each, the species' 5 bytes of bits after them;
        // HeartGold and SoulSilver's in overlay 1, 4 bytes each, the bits 8 bytes a species in
        // waza_oshie.bin; Black 2 and White 2's in overlay 36, 12 bytes each (the move, its cost in
        // shards, its place in the list), the bits in the personal records, a word a tutor from 0x3C.)
        {
            size_t at=0;
            if(b2)
            {
                const int first[3]={450,343,162};
                const Bytes overlay=RomOverlay(rom,36);
                // (Three tutors' lists one after another: Driftveil's 15, Nacrene's 15 and Lentimas's 17;
                // Humilau's 13 found by its own first moves, Bind, Snore and Knock Off. Their bits
                // in the personal records in the order Driftveil, Lentimas, Humilau, Nacrene.)
                auto list=[&](const int f[3],int count){size_t a=0;auto m=FindTutorMoves(overlay,f,12,count,a);return m;};
                const int humilauFirst[3]={20,173,282};
                auto shared=list(first,47),humilau=list(humilauFirst,13);
                if(shared.size()==47)
                {
                    tutorMoves=shared;tutorMoves.insert(tutorMoves.end(),humilau.begin(),humilau.end());
                    tutorBits.assign(personal.size(),0);
                    const int sizes[4]={15,17,13,15},bases[4]={0,30,humilau.size()==13?47:-1,15};
                    for(size_t sp=0;sp<personal.size();sp++)if(personal[sp].size()>=0x4C)
                        for(int l=0;l<4;l++)if(bases[l]>=0)
                        {
                            uint32_t w=U32(personal[sp].data()+0x3C+l*4);
                            for(int i=0;i<sizes[l];i++)if(w>>i&1)tutorBits[sp]|=1ull<<(bases[l]+i);
                        }
                }
            }
            else if(!gen5)
            {
                const int first[3]={291,189,210};
                if(hg)
                {
                    tutorMoves=FindTutorMoves(RomOverlay(rom,1),first,4,52,at);
                    auto bits=RomFile(rom,"fielddata/wazaoshie/waza_oshie.bin");
                    if(!tutorMoves.empty()&&bits.size()>=8)
                    {
                        tutorBits.assign(personal.size(),0);
                        for(size_t sp=1;sp<personal.size()&&Range(bits.size(),(sp-1)*8,8);sp++)
                            for(int i=0;i<52;i++)if(bits[(sp-1)*8+i/8]>>(i%8)&1)tutorBits[sp]|=1ull<<i;
                    }
                }
                else
                {
                    auto overlay=RomOverlay(rom,5);
                    tutorMoves=FindTutorMoves(overlay,first,12,38,at);
                    if(!tutorMoves.empty())
                    {
                        tutorBits.assign(personal.size(),0);
                        const size_t bits=at+38*12;
                        for(size_t sp=1;sp<personal.size()&&Range(overlay.size(),bits+(sp-1)*5,5);sp++)
                            for(int i=0;i<38;i++)if(overlay[bits+(sp-1)*5+i/8]>>(i%8)&1)tutorBits[sp]|=1ull<<i;
                    }
                }
            }
        }
        if(!gen5)dexSizes=Narc(RomFile(rom,hg?"a/0/7/4":"application/zukanlist/zkn_data/zukan_data_gira.narc"));
        if(dexSizes.size()>2)dexSizes.resize(2);
        // Only the members a front picture uses are kept (the archive also has backs and animations).
        battlePics=Narc(RomFile(rom,gen5?"a/0/0/4":hg?"a/0/0/4":"poketool/pokegra/pl_pokegra.narc"),gen5?[](size_t i){const size_t k=i%20;return k<=1||k>=18;}:[](size_t i){return i%6>=2;});
    }
};
}
