// Build actual wire frames for post-OTA reception checks, using production codecs.
#include "native_aprs.h"
#include "AprsCompress.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cassert>

void output(const char* label,const uint8_t* data,size_t n) {
    std::printf("%s\t",label);
    for(size_t i=0;i<n;i++) std::printf("%02x",data[i]);
    std::putchar('\n');
}
int main() {
    const std::string text="F4MLV-9>APLRG1:>OTA-TEXT-CHECK";
    std::string plain=std::string("\x3c\xff\x01")+text;
    output("text",reinterpret_cast<const uint8_t*>(plain.data()),plain.size());
    const std::string native="F4MLV-9>APLRG1,F4MLV-2*:>OTA-NATIVE-CHECK";
    NATIVE_APRS::RxtTuple tuple;tuple.index=0;std::memcpy(tuple.tuple,"bbbb",4);
    std::vector<uint8_t> wire;assert(NATIVE_APRS::encode(native,{tuple},wire));
    std::string back;std::vector<NATIVE_APRS::RxtTuple> tuples;
    assert(NATIVE_APRS::decode(wire.data(),wire.size(),back,tuples));
    assert(back==native && tuples.size()==1 && tuples[0].index==0 && !std::memcmp(tuples[0].tuple,"bbbb",4));
    output("native",wire.data(),wire.size());
    const std::string ppm="F4MLV-9>APLRG1:=L8gjuNmQ`# !GLoRa APRS firmware radio update validation OTA-PPM-CHECK";
    uint8_t compressed[255],restored[256];size_t n=AprsCompress::encodeFrame(reinterpret_cast<const uint8_t*>(ppm.data()),ppm.size(),compressed,sizeof compressed);
    assert(n && AprsCompress::decodeFrame(compressed,n,restored,sizeof restored)==ppm.size() && !std::memcmp(restored,ppm.data(),ppm.size()));
    output("ppm",compressed,n);
}
