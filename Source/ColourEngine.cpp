#include "ColourEngine.h"
#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#endif
#include "EmbeddedNam.h"
#include "NAM/wavenet/model.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>
#include <stdexcept>

namespace rack {
namespace {
constexpr int maxQuantum = 128, capacity = 8192, phases = 1024;
constexpr double pi = 3.14159265358979323846;
double dot(const double* a,const double* b,int n)noexcept {
#if defined(__APPLE__)
    double result=0;vDSP_dotprD(a,1,b,1,&result,(vDSP_Length)n);return result;
#else
    double result=0;for(int i=0;i<n;++i)result+=a[i]*b[i];return result;
#endif
}
// A normalized Blackman-windowed sinc, with cutoff reduced before decimation.
// Tables and all storage are prepared outside the audio callback.
struct Sinc {
    int half = 0;
    std::vector<double> coefficients;
    void prepare(double source, double destination) {
        half = (int)std::ceil(48.0 * std::max(1.0, source / destination));
        const double cutoff = 0.94 * std::min(1.0, destination / source);
        coefficients.resize((phases + 1) * half * 2);
        for(int p=0;p<=phases;++p) {
            double sum=0;
            for(int j=0;j<half*2;++j) {
                const double x=j-half+1-(double)p/phases;
                const double z=pi*x*cutoff;
                const double window=std::abs(x)>=half?0:0.42+0.5*std::cos(pi*x/half)+0.08*std::cos(2*pi*x/half);
                const double v=cutoff*(std::abs(z)<1e-12?1:std::sin(z)/z)*window;
                coefficients[p*half*2+j]=v;sum+=v;
            }
            for(int j=0;j<half*2;++j) coefficients[p*half*2+j]/=sum;
        }
    }
    double read(const std::array<double,capacity*2>& ring,double time) const noexcept {
        const auto base=(int64_t)std::floor(time);
        const double phase=(time-base)*phases;
        const int p=std::min(phases-1,(int)phase);
        const double f=phase-p;
        // Mirrored rings make the steady-state window contiguous. Accelerate's double
        // dot product uses SIMD; integer-ratio phases need only one dot product.
        if(base>=half-1) {
            const auto index=(size_t)(base-half+1)%capacity;
            const double* samples=ring.data()+index;
            const double a=dot(samples,coefficients.data()+p*half*2,half*2);
            if(f<1e-12)return a;
            const double b=dot(samples,coefficients.data()+(p+1)*half*2,half*2);
            return a+(b-a)*f;
        }
        double sum=0;
        for(int j=0;j<half*2;++j) {
            const int64_t index=base-half+1+j;
            if(index<0) continue;
            const double a=coefficients[p*half*2+j];
            const double b=coefficients[(p+1)*half*2+j];
            sum+=ring[(size_t)index%capacity]*(a+(b-a)*f);
        }
        return sum;
    }
};
}
struct ColourEngine::Impl {
    std::array<std::unique_ptr<nam::DSP>,2> models, referenceModels;
    // A matched capture at -6 dB supplies its common linear EQ/phase response.
    // Subtracting the scaled reference leaves level-dependent colour, which the
    // Enhance stage adds to the unfiltered dry path instead of a full tape blend.
    static constexpr double referenceScale = 0.5;
    bool nonlinearOnly = false;
    std::array<std::array<double,1024>,2> referenceInput{}, referenceOutput{};
    std::array<std::array<double,capacity*2>,2> input{}, neural{}, clean{}, algorithm{};
    std::array<std::array<double,1024>,2> modelInput{}, modelOutput{};
    std::array<std::array<float,maxQuantum>,2> pendingClean{}, pendingAlgo{}, pendingNeural{};
    std::array<std::array<std::array<float,maxQuantum>,2>,3> ready{};
    std::array<float,maxQuantum> pendingAmount{}, readyAmount{};
    std::array<float,capacity> amounts{};
    Sinc toModel,toHost;
    double rate=48000,ratio=1,preGain=1,postGain=1,dc=0;
    int quantum=16,delay=0,position=0,fadeLength=960,fadeRemaining=0,lastMode=0;
    int64_t host=0,nextModel=0;
    std::array<float,3> weights{1,0,0},steps{};
    std::array<float,2> latestClean{};
    bool converted=false;

    void render() noexcept {
        const int64_t first=host;
        for(int i=0;i<quantum;++i) for(int c=0;c<2;++c) {
            const auto index=(size_t)(host+i)%capacity;
            clean[c][index]=pendingClean[c][i];
            algorithm[c][index]=pendingAlgo[c][i];
            input[c][index]=pendingNeural[c][i]*preGain;
            input[c][index+capacity]=input[c][index];
            amounts[index]=pendingAmount[i];
        }
        host+=quantum;
        const int64_t modelFirst=nextModel;
        int count=0;
        while(nextModel*ratio+(converted?toModel.half:0)<=host-1) {
            for(int c=0;c<2;++c) modelInput[c][count]=converted?toModel.read(input[c],nextModel*ratio):input[c][(size_t)nextModel%capacity];
            ++nextModel;++count;
        }
        for(int c=0;c<2;++c) {
            double* in=modelInput[c].data(); double* out=modelOutput[c].data();
            if(count) models[c]->process(&in,&out,count);
            if(nonlinearOnly && count) {
                for(int i=0;i<count;++i) referenceInput[c][i]=in[i]*referenceScale;
                double* refIn=referenceInput[c].data();double* refOut=referenceOutput[c].data();
                referenceModels[c]->process(&refIn,&refOut,count);
            }
            for(int i=0;i<count;++i) {
                const auto index=(size_t)(modelFirst+i)%capacity;
                const double ac=out[i]-dc;
                neural[c][index]=(nonlinearOnly ? ac-(referenceOutput[c][i]-dc)/referenceScale : ac)*postGain;
                neural[c][index+capacity]=neural[c][index];
            }
        }
        for(int i=0;i<quantum;++i) {
            const int64_t time=first+i-delay;
            const double modelTime=time/ratio;
            for(int c=0;c<2;++c) {
                ready[0][c][i]=time<0?0:(float)algorithm[c][(size_t)time%capacity];
                ready[2][c][i]=time<0?0:(float)clean[c][(size_t)time%capacity];
                const double v=converted?toHost.read(neural[c],modelTime):(time<0?0:neural[c][(size_t)time%capacity]);
                ready[1][c][i]=std::isfinite(v)?(float)v:0.0f;
            }
            readyAmount[i]=time<0?0:amounts[(size_t)time%capacity];
        }
    }
};
ColourEngine::ColourEngine():impl(std::make_unique<Impl>()){}
ColourEngine::~ColourEngine()=default;
void ColourEngine::prepare(double sr,int model,int mode) {
    if(sr<8000 || sr>384000) throw std::runtime_error("NAM colour supports 8–384 kHz host rates");
    auto& x=*impl; x.rate=sr;x.ratio=sr/48000.;x.converted=std::abs(sr-48000)>0.01;
    // Keep at least 16 model-rate samples per processing call. Fixed 16-host-
    // sample batches shrink to four native samples at 192 kHz, making per-call
    // neural overhead dominate. The adapter delay stays fixed for each prepare.
    x.quantum=std::clamp((int)std::ceil(16*x.ratio),16,maxQuantum);
    if(x.converted){x.toModel.prepare(sr,48000);x.toHost.prepare(48000,sr);}
    x.delay=x.converted?(int)std::ceil(x.toModel.half+(x.toHost.half+2)*x.ratio):0;
    x.fadeLength=std::max(1,(int)(0.02*sr));
    x.preGain=model==1?0.177827941: model==3?0.354813389:1.0;
    x.nonlinearOnly=model==3;
    const auto config=nlohmann::json::parse(embeddedNam[model]);
    // Reference the fixed WaveNet factory explicitly. Static-archive AU linking
    // otherwise discards registration-only objects that the headless tool retains.
    nam::ScopedPrewarmOnResetDefault prewarmDefault(false);
    auto parsed=nam::wavenet::create_config(config.at("config"),48000);
    const auto weights=config.at("weights").get<std::vector<float>>();
    for(auto& m:x.models){m=parsed->create(weights,48000);m->Reset(48000,1024);}
    for(auto& m:x.referenceModels){m.reset();if(x.nonlinearOnly){m=parsed->create(weights,48000);m->Reset(48000,1024);m->prewarm();}}
    // Fixed small-signal calibration, not a running loudness normalizer: inter-module
    // drive and level-dependent harmonic behaviour remain intact.
    std::array<double,1024> in{},out{};double* ip=in.data();double* op=out.data();
    double energy=0,reference=0;
    for(int block=0;block<8;++block){
        for(int i=0;i<1024;++i)in[i]=0.01*x.preGain*std::sin(2*pi*1000*(block*1024+i)/48000.);
        x.models[0]->process(&ip,&op,1024);
        if(block>=4)for(int i=0;i<1024;++i){energy+=out[i]*out[i];reference+=(in[i]/x.preGain)*(in[i]/x.preGain);}
    }
    if(energy<1e-16 || !std::isfinite(energy))throw std::runtime_error("Invalid NAM calibration");
    x.postGain=std::sqrt(reference/energy);
    x.dc=0;
    for(auto& m:x.models){m->Reset(48000,1024);m->prewarm();}
    // Measure the profile's steady DC separately from its AC gain.
    in.fill(0);for(int b=0;b<8;++b)x.models[0]->process(&ip,&op,1024);
    for(auto v:out)x.dc+=v/1024.;
    for(auto& m:x.models){m->Reset(48000,1024);m->prewarm();}
    x.lastMode=std::clamp(mode,0,2);x.weights={0,0,0};x.weights[(size_t)x.lastMode]=1;
    reset();
}
void ColourEngine::reset() noexcept {
    auto& x=*impl;
    for(auto* rings:{&x.input,&x.neural,&x.clean,&x.algorithm})for(auto& r:*rings)r.fill(0);
    for(auto& m:x.ready)for(auto& c:m)c.fill(0);
    x.host=x.nextModel=0;x.position=0;x.fadeRemaining=0;
    x.readyAmount.fill(0);x.amounts.fill(0);
    x.weights={0,0,0};x.weights[(size_t)x.lastMode]=1;
}
int ColourEngine::latency()const noexcept{return impl->delay+impl->quantum;}
float ColourEngine::delayedClean(int c)const noexcept{return impl->latestClean[(size_t)c];}
void ColourEngine::frame(const float* clean,const float* algo,float* output,int channels,int mode,float amount,float namDrive)noexcept {
    auto& x=*impl;mode=std::clamp(mode,0,2);
    if(mode!=x.lastMode){x.lastMode=mode;x.fadeRemaining=x.fadeLength;for(int m=0;m<3;++m)x.steps[m]=((m==mode?1.f:0.f)-x.weights[m])/x.fadeLength;}
    if(x.fadeRemaining>0){for(int m=0;m<3;++m)x.weights[m]+=x.steps[m];if(--x.fadeRemaining==0){x.weights={0,0,0};x.weights[mode]=1;}}
    for(int c=0;c<2;++c){
        const float dry=x.ready[2][c][x.position];
        x.latestClean[c]=dry;
        const float neural=dry+x.readyAmount[x.position]*(x.ready[1][c][x.position]-(x.nonlinearOnly?0.f:dry));
        if(c<channels)output[c]=dry+x.weights[0]*(x.ready[0][c][x.position]-dry)+x.weights[1]*(neural-dry);
        x.pendingClean[c][x.position]=c<channels?clean[c]:clean[0];
        x.pendingAlgo[c][x.position]=c<channels?algo[c]:algo[0];
        x.pendingNeural[c][x.position]=(c<channels?clean[c]:clean[0])*namDrive;
    }
    x.pendingAmount[x.position]=amount;
    if(++x.position==x.quantum){x.render();x.position=0;}
}
}
