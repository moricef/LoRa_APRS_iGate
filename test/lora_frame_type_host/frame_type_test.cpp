#include "lora_frame_type.h"
#include <cassert>
#include <string>
#include <iostream>

int main() {
    const std::string tnc2="F4MLV-9>APLRG1:>TEST";
    auto accepts=[](const std::string& frame){
        return LORA_FRAME::mayReadAsText(reinterpret_cast<const uint8_t*>(frame.data()),frame.size());
    };
    assert(accepts(tnc2));
    assert(accepts(std::string("\x3c\xff\x01",3)+tnc2));
    assert(accepts(std::string("\x3c\xff\x01",3)+tnc2+std::string("\0\1abcd",6)));
    assert(!accepts(std::string("\x3c\xff",2)));
    for(unsigned type=0;type<256;type++) {
        std::string frame("\x3c\xff",2);frame+=char(type);
        assert(accepts(frame)==(type==1));
        // A plausible TNC2 payload must not bypass the type check.
        assert(accepts(frame+tnc2)==(type==1));
    }
    std::cout<<"256 frame types checked; text, hidden extension and legacy payload preserved\n";
}
