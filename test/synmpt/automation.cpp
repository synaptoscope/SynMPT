#include "../../common/stdafx.h"
#include "../../mptrack/PatternAutomation.h"
#include "../../soundlib/TrackRouting.h"
#include <iostream>
#include <stdexcept>

OPENMPT_NAMESPACE_BEGIN
void RunSynMPTAutomationChecks()
{
	auto check=[](bool ok,const char *message) { if(!ok) throw std::runtime_error(message); };
	const AutomationFormat it{true,true,true,255,1};
	ModCommand cell{};
	check(WriteAutomationValue(cell,AutomationLane::Pan,32,it)==AutomationEdit::Changed,"create pan");
	check(WriteAutomationValue(cell,AutomationLane::Volume,48,it)==AutomationEdit::Changed,"create gain beside pan");
	check(cell.volcmd==VOLCMD_PANNING && cell.vol==32 && cell.command==CMD_CHANNELVOLUME && cell.param==48,"both lanes fit in a native cell");
	check(ReadAutomationValue(cell,AutomationLane::Pan,it)==32 && ReadAutomationValue(cell,AutomationLane::Volume,it)==48,"read both lanes");
	check(EraseAutomationValue(cell,AutomationLane::Volume)==AutomationEdit::Changed && cell.volcmd==VOLCMD_PANNING,"erase gain keeps pan");
	check(!ReadAutomationValue(cell,AutomationLane::Volume,it),"last erased point has no gain automation");
	cell={};cell.note=NOTE_MIDDLEC;cell.instr=3;cell.volcmd=VOLCMD_VOLUME;cell.vol=47;
	check(!ReadAutomationValue(cell,AutomationLane::Volume,it),"note velocity must not open a gain lane");
	check(WriteAutomationValue(cell,AutomationLane::Pan,16,it)==AutomationEdit::Changed && cell.command==CMD_PANNING8,"pan uses free effect slot on a note");
	check(cell.note==NOTE_MIDDLEC && cell.instr==3 && cell.vol==47 && cell.volcmd==VOLCMD_VOLUME,"notes and velocity preserved");
	const auto original=cell;
	check(WriteAutomationValue(cell,AutomationLane::Volume,12,it)==AutomationEdit::Blocked && cell==original,"occupied slots preserved");
	cell={};cell.command=CMD_TEMPO;cell.param=150;
	check(WriteAutomationValue(cell,AutomationLane::Pan,11,it)==AutomationEdit::Changed && cell.command==CMD_TEMPO && cell.param==150,"pan keeps tempo");
	const auto withTempo=cell;
	check(WriteAutomationValue(cell,AutomationLane::Volume,22,it)==AutomationEdit::Blocked && cell==withTempo,"gain does not replace tempo");
	cell={};cell.note=NOTE_PCS;cell.instr=2;cell.volcmd=VOLCMD_VOLUME;cell.vol=4;cell.command=CMD_PANNING8;cell.param=37;
	const auto plugin=cell;
	check(WriteAutomationValue(cell,AutomationLane::Pan,0,it)==AutomationEdit::Blocked && cell==plugin,"plugin parameter notes protected");
	check(EraseAutomationValue(cell,AutomationLane::Pan)==AutomationEdit::Blocked && cell==plugin,"plugin parameter erase protected");
	cell={};cell.command=CMD_PANNING8;cell.param=128;
	check(WriteAutomationValue(cell,AutomationLane::Volume,29,it)==AutomationEdit::Changed && cell.volcmd==VOLCMD_PANNING && cell.vol==32 && cell.param==29,"native pan moves to free volume slot");
	const AutomationFormat s3m{true,true,false,128,1};
	cell={};check(WriteAutomationValue(cell,AutomationLane::Pan,32,s3m)==AutomationEdit::Changed && cell.param==64,"S3M center encoding");
	check(ReadAutomationValue(cell,AutomationLane::Pan,s3m)==32,"S3M center decoding");
	cell={};cell.command=CMD_PANNING8;cell.param=0xA4;
	check(EraseAutomationValue(cell,AutomationLane::Pan,s3m)==AutomationEdit::Unchanged && cell.param==0xA4,"S3M surround is not a pan automation point");
	const AutomationFormat xm{false,true,true,255,4};
	cell={};check(WriteAutomationValue(cell,AutomationLane::Pan,35,xm)==AutomationEdit::Changed && cell.vol==32,"XM legal panning precision");
	const auto xmCell=cell;
	check(WriteAutomationValue(cell,AutomationLane::Volume,30,xm)==AutomationEdit::Unsupported && cell==xmCell,"unsupported gain does not mutate data");

    check(AutomationLineValue({2,64},{18,0},10)==32,"descending line midpoint");
    std::vector<std::optional<int>> ramp(49);
    for(int row=0;row<49;row++) ramp[row]=AutomationLineValue({0,8},{48,61},row);
    const auto reduced=ReduceAutomationNodes(ramp,64);
    check(reduced.size()==2 && reduced.front().value==8 && reduced.back().row==48,"straight ramp has two editable endpoints after reload");
    ramp[24]=0;
    check(ReduceAutomationNodes(ramp,64).size()>2,"freehand peak remains a control point");
    std::vector<std::optional<int>> held(17);held[0]=0;held[16]=64;
    check(ReduceAutomationNodes(held,64).size()>=3,"sparse native commands retain held values");
    std::vector<ModChannelSettings> tracks(4);
    tracks[2].isBus=true;tracks[3].isBus=true;tracks[0].parentBus=2;tracks[1].parentBus=2;tracks[2].parentBus=3;
    check(TrackBelongsToBus(tracks,0,2) && TrackBelongsToBus(tracks,0,3),"nested group membership");
    const auto solo=TrackSoloSet(tracks,0,0);
    check(solo[0] && !solo[1] && solo[2] && solo[3],"solo child keeps parent buses audible");
    const auto busSolo=TrackSoloSet(tracks,2,2);
    check(busSolo[0] && busSolo[1] && busSolo[2] && busSolo[3],"solo bus includes child tracks and parent");
    auto gain=GetTrackBusGain(tracks,0,[](CHANNELINDEX){return std::tuple{32,128,false};});
    check(gain.left==16384 && gain.right==16384,"nested bus gains multiply");
    gain=GetTrackBusGain(tracks,0,[](CHANNELINDEX){return std::tuple{64,0,false};});
    check(gain.left==65536 && gain.right==0,"bus stereo balance");
    gain=GetTrackBusGain(tracks,0,[](CHANNELINDEX){return std::tuple{64,128,true};});
    check(gain.left==0 && gain.right==0,"bus mute");
    tracks[3].parentBus=2;
    check(!VisitTrackBuses(tracks,0,[](CHANNELINDEX){}),"cyclic routes terminate safely");
	std::cout << "PASS: native automation storage, shared lanes, note/velocity protection, effect collisions, plugin notes, erase, format mappings, line endpoints, nested buses, solo, gain/pan/mute and cycle protection\n";
}
OPENMPT_NAMESPACE_END
#ifdef SYNMPT_AUTOMATION_CHECK_MAIN
int main() { try { OPENMPT_NAMESPACE::RunSynMPTAutomationChecks(); } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; } }
#endif
