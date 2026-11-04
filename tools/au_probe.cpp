// Direct AU bundle regression check: bypasses system discovery but exercises the
// shipped factory, initialization, real audio rendering and global colour parameter.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static OSStatus input(void*,AudioUnitRenderActionFlags*,const AudioTimeStamp* time,
                      UInt32,UInt32 frames,AudioBufferList* data)
{
    for(UInt32 c=0;c<data->mNumberBuffers;++c){
        auto* p=static_cast<float*>(data->mBuffers[c].mData);
        if(!p)return kAudio_ParamError;
        for(UInt32 i=0;i<frames;++i)p[i]=(float)(0.03*std::sin(6.283185307179586*997*(time->mSampleTime+i)/96000.));
    }
    return noErr;
}
int main(int argc,char** argv)
{
    const char* path=argc>1?argv[1]:"/Library/Audio/Plug-Ins/Components/zychanstrip.component/Contents/MacOS/zychanstrip";
    auto* handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if(!handle){std::printf("FAIL dlopen: %s\n",dlerror());return 1;}
    auto factory=reinterpret_cast<AudioComponentFactoryFunction>(dlsym(handle,"zychanstripAUFactory"));
    if(!factory){std::puts("FAIL missing factory");return 1;}
    AudioComponentDescription desc{};desc.componentType='aufx';desc.componentSubType='Zych';desc.componentManufacturer='ZYUS';
    auto component=AudioComponentRegister(&desc,CFSTR("ZYUSOF: zychanstrip direct probe"),0x10101,factory);
    bool all=true;
    for(double rate:{44100.,48000.,96000.,192000.}){
        AudioUnit unit=nullptr;
        OSStatus open=AudioComponentInstanceNew(component,&unit);
        if(open){std::printf("FAIL open=%d\n",(int)open);return 1;}
        AudioStreamBasicDescription format{};format.mSampleRate=rate;format.mFormatID=kAudioFormatLinearPCM;
        format.mFormatFlags=kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved;
        format.mBytesPerPacket=4;format.mFramesPerPacket=1;format.mBytesPerFrame=4;format.mChannelsPerFrame=2;format.mBitsPerChannel=32;
        bool ok=AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format))==noErr;
        ok &= AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format))==noErr;
        UInt32 maxFrames=256;ok &= AudioUnitSetProperty(unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&maxFrames,sizeof(maxFrames))==noErr;
        AURenderCallbackStruct callback{input,nullptr};
        ok &= AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback))==noErr;
        OSStatus init=AudioUnitInitialize(unit);ok &= init==noErr;
        Float64 latency=0;UInt32 bytes=sizeof(latency);
        ok &= AudioUnitGetProperty(unit,kAudioUnitProperty_Latency,kAudioUnitScope_Global,0,&latency,&bytes)==noErr && latency>0;
        UInt32 listSize=0;Boolean writable=false;
        ok &= AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&listSize,&writable)==noErr;
        std::vector<AudioUnitParameterID> ids(listSize/sizeof(AudioUnitParameterID));
        ok &= AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,ids.data(),&listSize)==noErr;
        AudioUnitParameterID colour=0;bool found=false;
        for(auto id:ids){
            AudioUnitParameterInfo info{};UInt32 size=sizeof(info);
            if(AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterInfo,kAudioUnitScope_Global,id,&info,&size)!=noErr)continue;
            char name[256]{};
            if(info.flags&kAudioUnitParameterFlag_HasCFNameString)CFStringGetCString(info.cfNameString,name,sizeof(name),kCFStringEncodingUTF8);
            else std::strncpy(name,info.name,sizeof(name)-1);
            if(std::strcmp(name,"Colour Engine")==0){colour=id;found=true;}
            if((info.flags&kAudioUnitParameterFlag_CFNameRelease) && info.cfNameString)CFRelease(info.cfNameString);
        }
        ok &= found;
        float left[256]{},right[256]{};
        struct Buffers{UInt32 count;AudioBuffer buffers[2];} buffers{2,{{1,sizeof(left),left},{1,sizeof(right),right}}};
        AudioTimeStamp timestamp{};timestamp.mFlags=kAudioTimeStampSampleTimeValid;
        if(init==noErr && found)for(int mode=0;mode<3;++mode){
            ok &= AudioUnitSetParameter(unit,colour,kAudioUnitScope_Global,0,(AudioUnitParameterValue)mode,0)==noErr;
            double energy=0;
            for(int block=0;block<32;++block){
                AudioUnitRenderActionFlags flags=0;
                const auto status=AudioUnitRender(unit,&flags,&timestamp,0,256,reinterpret_cast<AudioBufferList*>(&buffers));
                ok &= status==noErr;
                for(int i=0;i<256;++i){ok &= std::isfinite(left[i]) && std::isfinite(right[i]);energy+=left[i]*left[i]+right[i]*right[i];}
                timestamp.mSampleTime+=256;
            }
            ok &= energy>1e-8;
        }
        std::printf("%s AU_BUNDLE rate=%.0f open=%d initialize=%d latency_ms=%.5f colour_found=%d modes=3\n",ok?"PASS":"FAIL",rate,(int)open,(int)init,latency*1000,(int)found);
        if(init==noErr)AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);all &= ok;
    }
    return all?0:1;
}
