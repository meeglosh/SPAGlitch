#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <cmath>
#include <unistd.h>
void check(OSStatus s,const char* what){if(s){fprintf(stderr,"%s: %d\n",what,(int)s);exit(1);}}
// macOS-only smoke test of the registered AU with a complete, local library.
int main(int argc,char**argv){if(argc!=2){fprintf(stderr,"Usage: AUAudioSmoke <local sample folder>\n");return 2;}@autoreleasepool{
AudioComponentDescription d={'aumu','Spgl','Spau',0,0};auto c=AudioComponentFindNext(nullptr,&d);if(!c)return 2;
AudioUnit u;check(AudioComponentInstanceNew(c,&u),"open");
CFPropertyListRef state=nullptr;UInt32 size=sizeof(state);check(AudioUnitGetProperty(u,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,&size),"get state");
NSMutableDictionary* dict=[(__bridge NSDictionary*)state mutableCopy];
NSString* key=nil;for(NSString* k in dict)if([dict[k] isKindOfClass:[NSData class]])key=k;
if(!key){NSLog(@"state keys %@",[dict allKeys]);return 3;}
NSString* xml=[NSString stringWithFormat:@"<SPAGlitch contentPath=\"%s\" singleSample=\"0\"/>",argv[1]];
NSData* text=[xml dataUsingEncoding:NSUTF8StringEncoding];uint32_t header[]={0x21324356,(uint32_t)text.length};NSMutableData* data=[NSMutableData dataWithBytes:header length:8];[data appendData:text];uint8_t zero=0;[data appendBytes:&zero length:1];dict[key]=data;
CFPropertyListRef modified=(__bridge CFPropertyListRef)dict;check(AudioUnitSetProperty(u,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&modified,sizeof(modified)),"restore");
AudioStreamBasicDescription format={48000,kAudioFormatLinearPCM,kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved,4,1,4,2,32,0};check(AudioUnitSetProperty(u,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format)),"format");check(AudioUnitInitialize(u),"initialize");
sleep(3);float left[512],right[512];struct{UInt32 n;AudioBuffer b[2];} buffers={2,{{1,sizeof(left),left},{1,sizeof(right),right}}};double peak=0;
check(MusicDeviceMIDIEvent(u,0x90,36,110,0),"note");for(int i=0;i<100;++i){AudioTimeStamp t={};t.mFlags=kAudioTimeStampSampleTimeValid;t.mSampleTime=i*512;AudioUnitRenderActionFlags f=0;check(AudioUnitRender(u,&f,&t,0,512,(AudioBufferList*)&buffers),"render");for(float v:left)peak=std::max(peak,(double)fabs(v));}
printf("Installed AU rendered peak: %.8f\n",peak);AudioComponentInstanceDispose(u);CFRelease(state);return peak>0?0:4;
}}
