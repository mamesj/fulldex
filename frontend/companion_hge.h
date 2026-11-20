#pragma once
// HG-Engine (github.com/BluRosie/hg-engine): the HeartGold engine hacks such as Heartfelt Gold are
// built on. What it changes in the ROM, read from the player's own ROM at run time. Hacks build
// different versions of the engine, which keep its tables in different places, so each table is
// found by its shape, not by where one version put it. Formats from hg-engine's own sources
// (GPL-3.0): include/constants/species.h, tools/source/speciesdatagen, scripts/build_learnsets.py,
// src/field/move_tutor.c and src/battle/battle_pokemon.c.
// No game data is bundled or written to disk.
// (Needs RomFile, Narc, Text and RomOverlay: companion_rom.h includes it after them.)
#include "companion_types.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
namespace Companion
{
struct HgEngine
{
    bool on=false;
    // Species: 1-493 as before, 494 the egg, 495 the bad egg, 496-543 unused, each later Pokemon at
    // its national number + 50 (Victini 544), then the forms (Megas, regional forms and the rest),
    // each with its own records, from formStart on.
    int count=0,newStart=0,formStart=0,nationalCount=493;
    std::vector<uint16_t> formBase;    // [id - formStart]: the species it is a form of
    // (Its form number in its species' own list (data/PokeFormDataTbl.c: 32 for each species, form 1
    // first, NEEDS_REVERSION 0x8000 on the forms only battles have, as Megas): 0 when not known.)
    std::vector<uint8_t> formNumber;std::vector<bool> formReverts;
    std::vector<std::vector<uint16_t>> speciesForms; // [species]: its forms, form 1 first
    std::vector<uint16_t> hidden;      // each species' hidden ability (the "code addons", a/0/2/8)
    // (Its personal records' abilities: two-byte at 0x16 and 0x1A since 2025; HeartGold's own bytes,
    // 0x16 and 0x17, in its earlier builds.)
    bool wideAbilities=true;
    std::vector<uint8_t> iconPalette;  // each species' icon palette
    std::vector<uint16_t> machines;    // the machine moves: TM01-92, HM01-08, then the later ones (MachineIndex)
    std::vector<std::string> machineLabels; // each one's item ("TM001", "HM01"; empty: no item)
    std::vector<int> machineItems;     // each one's item id (0: none)
    std::vector<uint16_t> tutors;      // the move tutors' moves (src/field/move_tutor.c)
    // (A row a species: machine and tutor bits, level-up moves {move, level} until 0xFFFF, egg moves
    // until 0xFFFF; one file each, every row the same size.)
    Bytes machineBits,tutorBits,learnsets,eggs;
    size_t machineStride=0,tutorStride=0,learnStride=0,eggStride=0;
    std::vector<std::string> heights,weights; // (the Pokedex's own text: "2'04\"", "15.2 lbs.")

    int Internal(int national)const{return national<=493||!newStart?national:newStart+national-494;}
    int National(int id)const
    {
        if(id>=1&&id<=493)return id;
        if(newStart&&id>=newStart&&id<formStart)return 494+id-newStart;
        return 0;
    }
    bool IsForm(int id)const{return on&&formStart&&id>=formStart&&id<count;}
    int Base(int id)const{return IsForm(id)&&id-formStart<(int)formBase.size()&&formBase[id-formStart]?formBase[id-formStart]:id;}
    std::vector<int> Forms(int species)const
    {
        std::vector<int> out;
        if(species<=0)return out;
        for(size_t i=0;i<formBase.size();i++)if(formBase[i]==species)out.push_back(formStart+(int)i);
        return out;
    }
    // (A form in use: one whose species is known; the engine leaves a few unused.)
    bool UsedForm(int id)const{return IsForm(id)&&Base(id)!=id;}
    int FormNumber(int id)const{return IsForm(id)&&id-formStart<(int)formNumber.size()?formNumber[id-formStart]:0;}
    bool Reverts(int id)const{return IsForm(id)&&id-formStart<(int)formReverts.size()&&formReverts[id-formStart];}
    // The record for a species' form (a party's or a trainer's Pokemon, an evolution's target);
    // the species itself when it has no such form.
    int FormSpecies(int species,int form)const
    {
        if(!on||form<=0||species<=0||species>=(int)speciesForms.size()||form>(int)speciesForms[species].size())return species;
        const int f=speciesForms[species][form-1];
        return f>0&&f<count?f:species;
    }
    static const uint8_t* Row(const Bytes& table,size_t stride,int id)
    {
        return stride&&id>=0&&Range(table.size(),(size_t)id*stride,stride)?table.data()+(size_t)id*stride:nullptr;
    }
    static bool Placeholder(const std::string& name){return name.find_first_not_of("- ")==std::string::npos;}
    // Tenths of a metre and of a kilogram from the Pokedex's feet and inches and pounds.
    bool Size(int id,int& height,int& weight)const
    {
        if(id<0||id>=(int)heights.size()||id>=(int)weights.size())return false;
        int feet=0,inches=0;float pounds=0;
        if(sscanf(heights[id].c_str()," %d'%d",&feet,&inches)!=2||sscanf(weights[id].c_str()," %f",&pounds)!=1)return false;
        if(feet<0||inches<0||pounds<=0)return false;
        height=(int)((feet*12+inches)*0.254f+0.5f);weight=(int)(pounds*4.5359237f+0.5f);
        return height>0;
    }
    bool Load(const Bytes& rom,const std::vector<Bytes>& personal,const std::vector<Bytes>& learnsetFiles,
              const std::vector<std::string>& names,size_t moveCount,const std::vector<std::string>& items,
              size_t abilityCount,const std::vector<Bytes>& msg)
    {
        on=false;
        // (The engine's: many more personal records than the 508 HeartGold has. Its level-up moves
        // one file, a row a species, since 2025; before that (Garbage Gold, Mythic Silver, HeartGold
        // Generations...) a file a species as HeartGold's, but each move a {move, level} word.)
        const bool rowLearnsets=learnsetFiles.size()==1;
        if(personal.size()<600||(!rowLearnsets&&learnsetFiles.size()<personal.size()))return false;
        if(!rowLearnsets)
        {
            // (Those words, not HeartGold's packed halfwords: Bulbasaur's to the Bulbasaur family's
            // last, every move a move and every level 0-100, each list ending 0xFFFF.)
            size_t good=0,all=0;
            for(size_t id=1;id<=9&&id<learnsetFiles.size();id++)
            {
                const Bytes& b=learnsetFiles[id];bool ended=false;
                for(size_t at=0;at+4<=b.size();at+=4)
                {
                    const unsigned move=U16(b.data()+at),level=U16(b.data()+at+2);
                    if(move==0xFFFF){ended=true;break;}
                    all++;good+=move>0&&move<moveCount&&level<=100;
                }
                if(!ended)return false;
            }
            if(!all||good*10<all*9)return false;
        }
        count=(int)personal.size();
        // (Two-byte abilities: 0x17 their high byte, 0 or 1 (abilities to 511); HeartGold's second
        // ability there instead, most species' some other.)
        {
            size_t second=0,seen=0;
            for(size_t id=1;id<=493&&id<personal.size();id++)if(personal[id].size()>0x1B){seen++;second+=personal[id][0x17]>1;}
            wideAbilities=!seen||second*10<seen;
        }
        newStart=0;
        for(int id=496;id<count&&id<(int)names.size();id++)if(!Placeholder(names[id])){newStart=id;break;}
        if(!newStart)return false;
        formStart=count;
        for(int id=newStart;id<count&&id<(int)names.size();id++)if(Placeholder(names[id])){formStart=id;break;}
        nationalCount=493+formStart-newStart;
        const auto addons=Narc(RomFile(rom,"a/0/2/8"));
        auto u16=[](const Bytes& b,size_t i){return (unsigned)U16(b.data()+i*2);};
        // Forms: the species each is a form of, one u16 for each form (0 for an unused one; a few
        // a form of another form, as a regional form's own forms).
        if(count>formStart)for(const auto& m:addons)
        {
            if(m.size()!=2*(size_t)(count-formStart))continue;
            size_t named=0;bool ok=true;
            for(size_t i=0;i<m.size()/2&&ok;i++){unsigned s=u16(m,i);ok=s<(unsigned)count;named+=s>0&&s<(unsigned)formStart;}
            if(!ok||named*10<m.size()/2*9)continue;
            for(size_t i=0;i<m.size()/2;i++)formBase.push_back((uint16_t)u16(m,i));
            for(auto& b:formBase)for(int hop=0;hop<4&&b>=formStart&&b-formStart<(int)formBase.size();hop++)b=formBase[b-formStart];
            break;
        }
        // Each species' forms in order: 32 u16 for each species, each a form's record, 0x8000 on the
        // ones that revert after battle; the species they name the same as the forms' own table.
        if(!formBase.empty())for(const auto& m:addons)
        {
            if(m.empty()||m.size()%64)continue;
            const size_t rows=m.size()/64;
            if(rows<494||rows>(size_t)count)continue;
            size_t entries=0,agree=0;
            for(size_t r=0;r<rows;r++)for(size_t k=0;k<32;k++)
            {
                const unsigned e=u16(m,r*32+k)&0x7FFF;if(!e)continue;
                entries++;
                if(e>=(unsigned)formStart&&e<(unsigned)count&&formBase[e-formStart]==r)agree++;
            }
            if(entries<50||agree*10<entries*9)continue;
            formNumber.assign(formBase.size(),0);formReverts.assign(formBase.size(),false);speciesForms.assign(rows,{});
            for(size_t r=0;r<rows;r++)for(size_t k=0;k<32;k++)
            {
                const unsigned raw=u16(m,r*32+k),e=raw&0x7FFF;
                if(!e)break;
                speciesForms[r].push_back((uint16_t)e);
                if(e>=(unsigned)formStart&&e<(unsigned)count){formNumber[e-formStart]=(uint8_t)(k+1);formReverts[e-formStart]=raw&0x8000;}
            }
            break;
        }
        // (Earlier builds (Mythic Silver, Shadows of Time) have neither table: instead a row for each
        // form, {its species, its form number (0x8000 on the ones that revert, in some), its record}.)
        if(speciesForms.empty()&&count>formStart)for(const auto& m:addons)
        {
            if(m.size()%6||m.size()/6<10)continue;
            const size_t n=m.size()/6;size_t good=0;
            auto valid=[&](size_t i){const unsigned s=u16(m,i*3),f=u16(m,i*3+1)&0x7FFF,rec=u16(m,i*3+2);return s>0&&s<(unsigned)formStart&&f>=1&&f<=32&&rec>=(unsigned)formStart&&rec<(unsigned)count;};
            for(size_t i=0;i<n;i++)good+=valid(i);
            if(good*10<n*9)continue;
            const bool own=formBase.empty();
            if(own)formBase.assign(count-formStart,0);
            formNumber.assign(count-formStart,0);formReverts.assign(count-formStart,false);speciesForms.assign(formStart,{});
            for(size_t i=0;i<n;i++)if(valid(i))
            {
                const unsigned s=u16(m,i*3),f=u16(m,i*3+1)&0x7FFF,rec=u16(m,i*3+2);
                if(own)formBase[rec-formStart]=(uint16_t)s;
                formNumber[rec-formStart]=(uint8_t)f;formReverts[rec-formStart]=u16(m,i*3+1)&0x8000;
                if(speciesForms[s].size()<f)speciesForms[s].resize(f,0);
                speciesForms[s][f-1]=(uint16_t)rec;
            }
            // (No reverting flag on any: not known, the Megas then told by their stats alone.)
            if(std::none_of(formReverts.begin(),formReverts.end(),[](bool r){return r;}))formReverts.clear();
            break;
        }
        // Hidden abilities: a u16 for each species, each one an ability, the same for each of the
        // first two families (Bulbasaur's, Charmander's).
        for(const auto& m:addons)
        {
            const size_t n=m.size()/2;
            if(m.size()%2||n<(size_t)formStart||n>(size_t)count+64)continue;
            bool ok=u16(m,1)&&u16(m,1)==u16(m,2)&&u16(m,2)==u16(m,3)&&u16(m,4)&&u16(m,4)==u16(m,5)&&u16(m,5)==u16(m,6);
            for(size_t i=0;i<n&&ok;i++)ok=u16(m,i)<abilityCount;
            if(ok){for(size_t i=0;i<n;i++)hidden.push_back((uint16_t)u16(m,i));break;}
        }
        // Icon palettes: a byte for each species, 0 to 5.
        for(const auto& m:addons)
            if(m.size()==(size_t)count&&std::all_of(m.begin(),m.end(),[](uint8_t b){return b<6;})){iconPalette=m;break;}
        // The machine moves: HM01-04 (Cut, Fly, Surf, Strength) at 92.
        for(const auto& m:addons)
        {
            const size_t n=m.size()/2;
            if(m.size()%2||n<100||n>1024||u16(m,92)!=15||u16(m,93)!=19||u16(m,94)!=57||u16(m,95)!=70)continue;
            bool ok=true;for(size_t i=0;i<n&&ok;i++)ok=u16(m,i)<moveCount;
            if(!ok)continue;
            for(size_t i=0;i<n;i++)machines.push_back((uint16_t)u16(m,i));
            break;
        }
        // (Each machine's item, as the engine's ItemToMachineMoveIndex (src/item.c) has them: TM001-092
        // and HM01-08 (items 328-427) at 0-99, Omega Ruby's HM07 (Dive) at 100, TM00 at 101, TM093-095
        // at 102, TM096-100 at 105, Scarlet and Violet's TM100 on at 110 (the second item of that
        // name), TR00-99 at 240; the later items found by their names.)
        machineLabels.assign(machines.size(),std::string());machineItems.assign(machines.size(),0);
        for(size_t i=0;i<100&&i<machines.size()&&328+i<items.size();i++){machineLabels[i]=items[328+i];machineItems[i]=328+(int)i;}
        int hundreds=0;
        for(int id=428;id<(int)items.size();id++)
        {
            const std::string& n=items[id];
            if(n.size()<4||!std::isdigit((unsigned char)n[2])||!std::all_of(n.begin()+2,n.end(),[](char ch){return std::isdigit((unsigned char)ch);}))continue;
            const int number=atoi(n.c_str()+2);int index=-1;
            if(n[0]=='H'&&n[1]=='M')index=100;
            else if(n[0]=='T'&&n[1]=='R')index=240+number;
            else if(n[0]=='T'&&n[1]=='M')
            {
                if(n=="TM00")index=101;
                else if(number>=93&&number<=95)index=102+number-93;
                else if(number>=96&&number<=99)index=105+number-96;
                else if(number==100)index=hundreds++==0?109:110;
                else if(number>=101&&number<=229)index=110+number-100;
            }
            if(index>=0&&index<(int)machines.size()&&machineLabels[index].empty()){machineLabels[index]=n;machineItems[index]=id;}
        }
        // From the overlays, in one pass:
        // - the tutors' moves: Dive, Mud-Slap, Fury Cutter... four bytes each (the move, its cost, the
        //   tutor), the longest such list (HeartGold's own, in overlay 1, is shorter);
        // - the type chart: the battle code's own {attack, defence, multiplier} table, Normal against
        //   Rock first, with the Fairy type (9) in it (HeartGold's own, without Fairy, is left alone).
        {
            const size_t overlays=rom.size()>=0x58?U32(rom.data()+0x54)/32:0;
            const uint8_t start[6]={0,5,5,0,8,5};
            for(size_t ov=0;ov<overlays&&ov<512;ov++)
            {
                const Bytes o=RomOverlay(rom,(unsigned)ov);
                for(size_t at=0;Range(o.size(),at,12);at+=2)
                {
                    if(U16(o.data()+at)!=291||U16(o.data()+at+4)!=189||U16(o.data()+at+8)!=210)continue;
                    std::vector<uint16_t> list;
                    for(size_t e=at;Range(o.size(),e,4)&&list.size()<512;e+=4)
                    {
                        const unsigned move=U16(o.data()+e);
                        if(!move||move>=moveCount||o[e+3]>8)break;
                        list.push_back((uint16_t)move);
                    }
                    if(list.size()>tutors.size())tutors=list;
                }
                for(size_t at=0;CompanionTypes::romChart.empty()&&Range(o.size(),at,6);at++)
                {
                    if(memcmp(o.data()+at,start,6))continue;
                    std::vector<std::array<uint8_t,3>> chart;bool ok=false,fairyType=false;
                    for(size_t e=at;Range(o.size(),e,3)&&chart.size()<256;e+=3)
                    {
                        if(o[e]==0xFF){ok=true;break;}
                        if(o[e]==0xFE||o[e]==0xFD)continue; // (markers: the immunities a Ring Target and Foresight lift follow)
                        if(o[e]>=32||o[e+1]>=32||(o[e+2]!=0&&o[e+2]!=5&&o[e+2]!=10&&o[e+2]!=20))break;
                        if(o[e]>=18||o[e+1]>=18)continue; // (a hack's own types past Dark (Iridium's): no Pokemon has them here)
                        fairyType=fairyType||o[e]==9||o[e+1]==9;
                        chart.push_back({o[e],o[e+1],o[e+2]});
                    }
                    if(ok&&fairyType&&chart.size()>=100){CompanionTypes::romChart=chart;CompanionTypes::fairy=true;}
                }
                // - (earlier builds) the icon palettes: not in the code addons but at the start of
                //   the engine's own overlay (129), a byte for each species, most of HeartGold's for the
                //   first 493 (the engine's newer icons change about one in six, as Weedle's).
                for(size_t at=0;iconPalette.empty()&&Range(o.size(),at,(size_t)count);at++)
                {
                    if(o[at]!=0||o[at+1]!=CompanionIcons::Gen4Palette[1]||o[at+4]!=CompanionIcons::Gen4Palette[4]||o[at+8]!=CompanionIcons::Gen4Palette[8])continue;
                    size_t same=0;for(size_t sp=1;sp<=493;sp++)same+=o[at+sp]==CompanionIcons::Gen4Palette[sp];
                    if(same<345||!std::all_of(o.begin()+at,o.begin()+at+count,[](uint8_t b){return b<6;}))continue;
                    iconPalette.assign(o.begin()+at,o.begin()+at+count);
                }
            }
            // (No chart found, but Clefairy is type 9: the Fairy type by name at least.)
            if(personal.size()>35&&personal[35].size()>7&&personal[35][6]==9)CompanionTypes::fairy=true;
        }
        // (A table of rows of one size, as many rows as species or a few more.)
        auto rows=[&](size_t stride,const Bytes* not1)->const Bytes*
        {
            if(!stride)return nullptr;
            for(const auto& m:addons)
            {
                if(&m==not1||m.empty()||m.size()%stride)continue;
                const size_t n=m.size()/stride;
                if(n>=(size_t)count&&n<=(size_t)count+64)return &m;
            }
            return nullptr;
        };
        size_t rowCount=0;
        if(!machines.empty())
        {
            machineStride=(machines.size()+31)/32*4;
            if(const Bytes* m=rows(machineStride,nullptr)){machineBits=*m;rowCount=m->size()/machineStride;}
            if(!tutors.empty())
            {
                tutorStride=(tutors.size()+31)/32*4;
                const Bytes* machineTable=nullptr;for(const auto& m:addons)if(m==machineBits){machineTable=&m;break;}
                if(const Bytes* t=rows(tutorStride,machineTable))tutorBits=*t;
                else tutorStride=0;
            }
        }
        // Level-up moves and egg moves: one file each, a row for each of those species.
        if(rowLearnsets)
        {
            const Bytes& level=learnsetFiles[0];
            if(rowCount&&level.size()%rowCount==0&&level.size()/rowCount%4==0){learnsets=level;learnStride=level.size()/rowCount;}
            else if(level.size()%136==0){learnsets=level;learnStride=136;}
        }
        else
        {
            // (Earlier builds: a file each, made rows of the longest one's size, 0xFFFF after each list.)
            size_t stride=4;for(const auto& b:learnsetFiles)stride=std::max(stride,(b.size()+3)/4*4+4);
            learnsets.assign(stride*learnsetFiles.size(),0xFF);learnStride=stride;
            for(size_t i=0;i<learnsetFiles.size();i++)if(!learnsetFiles[i].empty())memcpy(learnsets.data()+i*stride,learnsetFiles[i].data(),learnsetFiles[i].size());
        }
        const auto eggFiles=Narc(RomFile(rom,"a/2/2/9"));
        // (Earlier builds keep HeartGold's own list, 20000 + the species before each one's moves.)
        if(rowLearnsets&&eggFiles.size()==1)
        {
            const Bytes& e=eggFiles[0];
            if(rowCount&&e.size()%rowCount==0&&e.size()/rowCount%2==0){eggs=e;eggStride=e.size()/rowCount;}
            else if(e.size()%48==0){eggs=e;eggStride=48;}
        }
        // Heights and weights: the Pokedex's own text for each species, in feet and inches and in
        // pounds (HeartGold's own table, a/0/7/4, stops at 493); one for each record, or (earlier
        // builds) for each species to the forms.
        for(const auto& bank:msg)
        {
            if(bank.size()<4||(U16(bank.data())!=(unsigned)count&&U16(bank.data())!=(unsigned)formStart))continue;
            auto text=Text(bank,false);
            if(text.size()<2)continue;
            if(heights.empty()&&text[1].find('\'')!=std::string::npos&&text[1].find('"')!=std::string::npos)heights=std::move(text);
            else if(weights.empty()&&text[1].find("lbs")!=std::string::npos)weights=std::move(text);
            if(!heights.empty()&&!weights.empty())break;
        }
        on=true;
        return true;
    }
};
}
