/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "Mptrack.h"
#include "../soundlib/ModChannel.h"
#include <fstream>
#include <sstream>
#include <algorithm>

OPENMPT_NAMESPACE_BEGIN
inline std::string &SynMPTColorRules()
{
    static std::string rules = [] {
        std::ifstream input((theApp.GetConfigPath() + P_("SynMPT-colors.txt")).AsNative());
        if(input) return std::string(std::istreambuf_iterator<char>(input), {});
        return std::string("1|kick|#CE765C\n1|snare|#DCAC64\n1|drum|#D59663\n1|bass|#86A8D8\n1|guitar|#89B78B\n1|lead|#B594D4\n1|pad|#73B7B4\n");
    }(); return rules;
}
inline bool SynMPTParseColorRule(const std::string &line, std::string &keyword, COLORREF &color, bool &enabled)
{
    const auto a=line.find('|'), b=line.rfind('|');
    if(a==std::string::npos || a==b || b+8!=line.size() || line[b+1]!='#' || (line.substr(0,a)!="0" && line.substr(0,a)!="1")) return false;
    keyword=line.substr(a+1,b-a-1); if(keyword.empty()) return false;
    const auto hex=line.substr(b+2); if(hex.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos) return false;
    unsigned value=0; std::istringstream(hex) >> std::hex >> value;
    color=RGB((value>>16)&255,(value>>8)&255,value&255); enabled=line[0]=='1';
    std::transform(keyword.begin(),keyword.end(),keyword.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));}); return true;
}
inline void SynMPTApplyAutoColor(ModChannelSettings &track)
{
    if(!track.trackAutoColor) return;
    std::string name=track.szName; std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    std::istringstream lines(SynMPTColorRules()); std::string line;
    while(std::getline(lines,line))
    {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        std::string key; COLORREF color; bool enabled;
        if(SynMPTParseColorRule(line,key,color,enabled) && enabled && name.find(key)!=std::string::npos) { track.color=color; return; }
    }
}
OPENMPT_NAMESPACE_END
