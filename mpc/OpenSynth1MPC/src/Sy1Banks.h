// Sy1Banks.h - bounded, constructor-only parser for original Synth1 .sy1 files.
// Original .sy1 format documented and implemented in letsdig/OpenSynth1 (MIT).
// No filesystem operations occur in the realtime audio callback.
#pragma once
#include "mpc_plugin.h"
#include <array>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <vector>
#include <sys/stat.h>
namespace sy1 {
struct Patch {
    std::string bank, name;
    std::array<int,128> value{};
    std::array<bool,128> present{};
};
inline bool isPatch(const std::string& name) {
    if (name.size() < 5) return false;
    std::string ext=name.substr(name.size()-4);
    for(auto& c:ext)c=(char)std::tolower((unsigned char)c);
    return ext==".sy1";
}
inline std::string trim(std::string s) {
    auto notSpace=[](unsigned char c){return !std::isspace(c);};
    auto b=std::find_if(s.begin(),s.end(),notSpace);
    if(b==s.end())return {};
    auto e=std::find_if(s.rbegin(),s.rend(),notSpace).base();
    return std::string(b,e);
}
inline bool parse(const std::string& path,const std::string& bank,Patch& out){
    std::string text;
    if(!mpc::readTextFile(path,text) || text.empty() || text.size()>65536)return false;
    out=Patch{};
    out.bank=bank;
    size_t p=0;int lineNo=0,parameters=0;
    while(p<text.size()){
        auto e=text.find('\n',p);
        if(e==std::string::npos)e=text.size();
        std::string line=trim(text.substr(p,e-p));
        p=e<text.size()?e+1:e; ++lineNo;
        if(lineNo==1){out.name=line.substr(0,60);continue;}
        if(line.empty() || line.rfind("color=",0)==0 || line.rfind("ver=",0)==0)continue;
        auto comma=line.find(',');
        if(comma==std::string::npos)continue;
        std::string left=trim(line.substr(0,comma)),right=trim(line.substr(comma+1));
        if(left.empty()||right.empty())continue;
        char *end=nullptr;
        long id=std::strtol(left.c_str(),&end,10);
        if(end==left.c_str()||*end!='\0'||id<0||id>=128)continue;
        long val=std::strtol(right.c_str(),&end,10);
        if(end==right.c_str()||*end!='\0'||val<-32768||val>65535)continue;
        out.value[(size_t)id]=(int)val;
        if(!out.present[(size_t)id])++parameters;
        out.present[(size_t)id]=true;
    }
    if(out.name.empty())out.name="Untitled";
    return parameters>=5;
}
inline std::vector<std::string> list(const std::string& dir,bool dirs){
    std::vector<std::string> ret;
    DIR* d=opendir(dir.c_str());if(!d)return ret;
    while(const dirent* e=readdir(d)){
        std::string n(e->d_name);
        if(n=="."||n==".."||n.empty()||n[0]=='.')continue;
        if(!dirs&&!isPatch(n))continue;
        struct stat st{};
        if(stat((dir+"/"+n).c_str(),&st)!=0)continue;
        if(dirs?S_ISDIR(st.st_mode):S_ISREG(st.st_mode))ret.push_back(n);
    }
    closedir(d);std::sort(ret.begin(),ret.end());
    return ret;
}
// Read up to 512 file presets per plugin instance; max one bank-folder level.
// Scanning is constructor-only and never called from process().
inline std::vector<Patch> load(const std::string& root,int maxPatches=512){
    std::vector<Patch> patches;
    auto collect=[&](const std::string& path,const std::string& bank){
        for(const auto& n:list(path,false)){
            if((int)patches.size()>=maxPatches)break;
            Patch v;
            if(parse(path+"/"+n,bank,v))patches.push_back(std::move(v));
        }
    };
    collect(root,"Loose");
    for(const auto& bank:list(root,true)){
        if((int)patches.size()>=maxPatches)break;
        collect(root+"/"+bank,bank);
    }
    return patches;
}
} // namespace sy1
