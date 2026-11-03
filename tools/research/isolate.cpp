// Bit-width decomposition to find which module introduces the residual error.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../../Source/Modules.h"

namespace {
constexpr double SR = 48000.0; constexpr int B = 256; constexpr int SECS = 4; constexpr int CH = 2;

struct Chain {
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<rack::ConsoleModule> console; std::unique_ptr<rack::OptoModule> opto;
    std::unique_ptr<rack::FetModule> fet; std::unique_ptr<rack::LimiterModule> limiter;
    rack::Module* byId[4]{};
    void build() {
        using namespace rack;
        auto v=[this](const std::string& i,float x){apvts.set(i,x);};
        v("con_bypass",0);v("con_hpf",20);v("con_lf",0);v("con_lmf_f",600);v("con_lmf",0);v("con_hmf_f",3000);
        v("con_hmf",0);v("con_hf",0);v("con_thresh",0);v("con_ratio",3);v("con_attack",10);v("con_release",200);v("con_out",0);
        v("opto_bypass",0);v("opto_peak",0);v("opto_gain",50);v("opto_mode",0);
        v("fet_bypass",0);v("fet_input",0);v("fet_output",0);v("fet_attack",3);v("fet_release",4);v("fet_ratio",0);
        v("lim_bypass",0);v("lim_gain",0);v("lim_ceiling",-0.3f);v("lim_release",100);v("lim_enhance",0);v("lim_auto_gain",1);
        for (auto* id : Eq560::bandIds) v(std::string("con_")+id, 0);
        console=std::make_unique<ConsoleModule>(apvts,"con_"); opto=std::make_unique<OptoModule>(apvts,"opto_");
        fet=std::make_unique<FetModule>(apvts,"fet_"); limiter=std::make_unique<LimiterModule>(apvts,"lim_");
        byId[0]=console.get();byId[1]=opto.get();byId[2]=fet.get();byId[3]=limiter.get();
        for(auto*m:byId) m->prepare(SR,B);
    }
    static constexpr std::array<int,4> order{0,2,1,3};
};
}

int main(int argc, char** argv)
{
    // argv[1]: module to run exclusively ("" = chain); argv[2]: output file
    const std::string only = argc>1?argv[1]:"";
    const std::string outPath = argc>2?argv[2]:"/dev/null";
    Chain c; c.build();
    const int total=(int)(SR*SECS), blocks=total/B;
    std::vector<float> L(total),R(total);
    uint32_t rng=0x12345678u;
    auto noise=[&rng]{rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return ((float)(rng&0xffffff)/(float)0x800000)-1.0f;};
    for(int i=0;i<total;++i){
        double t=(double)i/SR, burst=std::fmod(t,0.25)<0.05?1.0:0.25;
        double s=0.30*std::sin(2.0*M_PI*100.0*t)+0.30*std::sin(2.0*M_PI*1000.0*t)
                +0.20*std::sin(2.0*M_PI*8000.0*t)*burst+0.01*noise();
        L[i]=(float)s; R[i]=(float)(s*0.9+0.02*noise());
    }
    juce::AudioBuffer<float> buf(CH,B);
    rack::Module* single=nullptr;
    if(only=="console") single=c.console.get(); else if(only=="opto") single=c.opto.get();
    else if(only=="fet") single=c.fet.get(); else if(only=="limiter") single=c.limiter.get();
    FILE* f=fopen(outPath.c_str(),"wb");
    for(int b=0;b<blocks;++b){
        for(int ch=0;ch<CH;++ch) std::copy_n((ch?R:L).data()+(size_t)b*B,B,buf.getWritePointer(ch));
        if(single) single->run(buf); else for(int id:Chain::order) c.byId[id]->run(buf);
        if(b>=blocks/4) for(int i=0;i<B;++i) for(int ch=0;ch<CH;++ch){float s=buf.getSample(ch,i);fwrite(&s,4,1,f);}
    }
    fclose(f);
    return 0;
}
