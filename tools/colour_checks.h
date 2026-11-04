#pragma once
// Integration checks use the production colour engine, including real embedded models.
void colourTests()
{
    for(double sr : {44100.,48000.,88200.,96000.,176400.,192000.}) {
        rack::ColourEngine engine;
        engine.prepare(sr,0,2);
        const int latency=engine.latency();
        bool aligned=true;
        for(int i=0;i<latency+256;++i){
            float raw[2]{i==0?0.1f:0.0f,i==0?0.2f:0.0f},out[2]{};
            engine.frame(raw,raw,out,2,2);
            aligned &= std::abs(out[0]-(i==latency?0.1f:0.0f))<1e-7f;
            aligned &= std::abs(out[1]-(i==latency?0.2f:0.0f))<1e-7f;
        }
        check(aligned,"CLEAN exact delayed impulse @"+juce::String(sr)+" latency="+juce::String(latency));
    }
    for(int model=0;model<4;++model) {
        rack::ColourEngine engine;engine.prepare(48000,model,1);
        double power=0;bool finite=true;float previous=0,maxJump=0;
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<48000;++i) {
            float raw[2]{(float)(0.1*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000.)),0.0f},out[2]{};
            raw[1]=raw[0];
            const int mode=i<24000?1:i<36000?2:0;
            engine.frame(raw,raw,out,2,mode);
            finite &= std::isfinite(out[0]) && std::isfinite(out[1]);
            finite &= std::abs(out[0]-out[1])<1e-6f;
            if(i>4096 && i<24000)power+=out[0]*out[0];
            if(i>23000)maxJump=std::max(maxJump,std::abs(out[0]-previous));
            previous=out[0];
        }
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"NAM model "<<model<<" stereo 48k: "<<ms<<" ms per audio second\n";
        check(finite && power>1e-4,"NAM model "+juce::String(model)+" produces finite nonzero linked stereo audio");
        check(maxJump<0.08f,"colour mode switching stays bounded for model "+juce::String(model));
    }
    {
        rack::ColourEngine engine;engine.prepare(96000,0,1);
        double power=0;int samples=0;
        for(int i=0;i<24000;++i){
            float raw[2]{(float)(0.1*std::sin(2*juce::MathConstants<double>::pi*30000*i/96000.)),0},out[2]{};
            engine.frame(raw,raw,out,1,1);
            if(i>4096){power+=out[0]*out[0];++samples;}
        }
        const double rms=std::sqrt(power/samples);
        check(rms<1e-4,"96k NAM input rejects 30kHz before decimation (RMS "+juce::String(rms,8)+")");
    }
    for(double sr : {44100.,48000.,88200.,96000.,176400.,192000.}) {
        ZychanStripProcessor bench;
        setParam(bench,"colour_mode",1);setParam(bench,"lim_auto_gain",0);
        setParam(bench,"lim_enhance",30);
        prepare(bench,2,sr);
        juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;
        const int blocks=(int)(sr*0.25/128);
        bool finite=true;
        const auto start=std::chrono::steady_clock::now();
        for(int block=0;block<blocks;++block){
            for(int i=0;i<128;++i)for(int c=0;c<2;++c)b.setSample(c,i,(float)(0.03*std::sin(2*juce::MathConstants<double>::pi*(c?1733:997)*(block*128+i)/sr)));
            bench.processBlock(b,midi);
            for(int c=0;c<2;++c)for(int i=0;i<128;++i)finite &= std::isfinite(b.getSample(c,i)) && std::abs(b.getSample(c,i))<=1;
        }
        const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"RACK_BENCH rate="<<sr<<" latency="<<bench.getLatencySamples()<<" ms="<<1000.0*bench.getLatencySamples()/sr<<" cpu_ms_per_second="<<elapsed*sr/(blocks*128)<<"\n";
        check(finite,"full NAM rack finite bounded output @"+juce::String(sr));
    }
    ZychanStripProcessor proc;
    auto* mode=proc.apvts.getParameter("colour_mode");
    check(mode!=nullptr,"global colour parameter exists");
    setParam(proc,"colour_mode",2);
    juce::MemoryBlock state;proc.getStateInformation(state);
    ZychanStripProcessor restored;restored.setStateInformation(state.getData(),(int)state.getSize());
    check(restored.apvts.getRawParameterValue("colour_mode")->load()==2,"colour mode recalls with session");
    {
        auto old=proc.apvts.copyState();
        old.removeChild(old.getChildWithProperty("id","colour_mode"),nullptr);
        juce::MemoryBlock blob;juce::AudioProcessor::copyXmlToBinary(*old.createXml(),blob);
        restored.setStateInformation(blob.getData(),(int)blob.getSize());
        check(restored.apvts.getRawParameterValue("colour_mode")->load()==0,"old sessions migrate to ALGO");
    }
    prepare(proc,2,96000);
    const int latency=proc.getLatencySamples();
    bool constant=true;
    for(int m=0;m<3;++m){setParam(proc,"colour_mode",(float)m);juce::AudioBuffer<float>b(2,256);b.clear();juce::MidiBuffer midi;proc.processBlock(b,midi);constant &= proc.getLatencySamples()==latency;}
    check(constant,"mode switch keeps rack latency constant");
    bool allOrders=true;
    std::array<int,4> order{0,1,2,3};
    bypassAll(proc);
    do {
        proc.setOrder(order);
        for(int m=0;m<3;++m){
            setParam(proc,"colour_mode",(float)m);
            juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;
            for(int block=0;block<24;++block){
                for(int c=0;c<2;++c)juce::FloatVectorOperations::fill(b.getWritePointer(c),0.03f*(c+1),128);
                proc.processBlock(b,midi);
            }
            allOrders &= proc.getLatencySamples()==latency;
            for(int c=0;c<2;++c)for(int i=0;i<128;++i)allOrders &= std::abs(b.getSample(c,i)-0.03f*(c+1))<1e-6f;
        }
    }while(std::next_permutation(order.begin(),order.end()));
    check(allOrders,"all 24 rack orders preserve bypass and latency in all three modes");
    for(const char* id:{"con_bypass","fet_bypass","opto_bypass","lim_bypass"})setParam(proc,id,0);
    setParam(proc,"lim_auto_gain",0);setParam(proc,"lim_enhance",30);
    order={0,1,2,3};bool activeOrders=true;
    do {
        proc.setOrder(order);
        for(int m=0;m<3;++m){
            setParam(proc,"colour_mode",(float)m);
            juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;
            for(int block=0;block<24;++block){
                for(int i=0;i<128;++i)for(int c=0;c<2;++c)b.setSample(c,i,(float)(0.01*std::sin(2*juce::MathConstants<double>::pi*997*(block*128+i)/96000.)));
                proc.processBlock(b,midi);
                for(int c=0;c<2;++c)for(int i=0;i<128;++i)activeOrders &= std::isfinite(b.getSample(c,i)) && std::abs(b.getSample(c,i))<1;
            }
        }
    }while(std::next_permutation(order.begin(),order.end()));
    check(activeOrders,"all 24 active rack orders produce finite bounded audio in all three modes");
    proc.setOrder({0,2,1,3});
    for(const char* id:{"con_bypass","fet_bypass","opto_bypass"})setParam(proc,id,1);
    setParam(proc,"colour_mode",1);setParam(proc,"lim_ceiling",-6);setParam(proc,"lim_gain",18);setParam(proc,"lim_enhance",100);
    bool ceiling=true;
    for(int block=0;block<100;++block){
        juce::AudioBuffer<float>b(2,128);juce::MidiBuffer midi;
        for(int i=0;i<128;++i)for(int c=0;c<2;++c)b.setSample(c,i,(float)(0.9*std::sin(2*juce::MathConstants<double>::pi*997*(block*128+i)/96000.)));
        proc.processBlock(b,midi);
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)ceiling &= std::isfinite(b.getSample(c,i)) && std::abs(b.getSample(c,i))<=0.501188f+1e-6f;
    }
    check(ceiling,"limiter ceiling holds after maximum NAM Enhance drive");



}

void colourGuiTests(ZychanStripProcessor& proc,ZychanStripEditor& editor)
{
    auto* selector=editor.getColourSelectorForTest();
    check(selector!=nullptr && selector->isVisible(),"colour selector visible on rack");
    setParam(proc,"colour_mode",0);
    selector->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
    check(proc.apvts.getRawParameterValue("colour_mode")->load()==1,"colour selector writes host parameter");
    editor.setMeterPageVisible(true);
    check(selector->isVisible() && editor.getLocalBounds().contains(selector->getBounds()),"colour selector visible inside meter page");
    editor.setMeterPageVisible(false);
    setParam(proc,"colour_mode",0);
}
