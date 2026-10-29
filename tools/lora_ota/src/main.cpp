// Local OTA bench, GPL-3.0-or-later. No production configuration is read or changed.
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include "session.h"

using namespace labota;
SX1278 radio = new Module(18,26,14,RADIOLIB_NC);
volatile bool received=false;
void IRAM_ATTR onReceive() { received=true; }
uint8_t secret[32]{}, nonce[8]{};
String target;
bool armed=false;
uint32_t armedAt=0,lastPacket=0;
char input[640]; size_t inputSize=0;

class FlashStore : public Store {
    const esp_partition_t* partition=nullptr;
    esp_ota_handle_t handle=0;
    uint32_t bytes=0;
public:
    uint32_t capacity() const override {
        auto p=esp_ota_get_next_update_partition(nullptr); return p?p->size:0;
    }
    bool begin(uint32_t size) override {
        partition=esp_ota_get_next_update_partition(nullptr); bytes=size;
        return partition && esp_ota_begin(partition,size,&handle)==ESP_OK;
    }
    bool write(const uint8_t* p,size_t n) override { return esp_ota_write(handle,p,n)==ESP_OK; }
    bool same(uint32_t offset,const uint8_t* p,size_t n) override {
        uint8_t back[CHUNK];
        return n<=sizeof back && esp_partition_read(partition,offset,back,n)==ESP_OK && !memcmp(back,p,n);
    }
    bool finish(const uint8_t expected[32]) override {
        // Check bytes read back from flash, not just the input buffers.
        mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts_ret(&ctx,0);
        uint8_t block[512],digest[32]; bool ok=true;
        for(uint32_t pos=0;pos<bytes && ok;) {
            size_t n=std::min(size_t(bytes-pos),sizeof block);
            ok=esp_partition_read(partition,pos,block,n)==ESP_OK;
            if(ok) ok=mbedtls_sha256_update_ret(&ctx,block,n)==0;
            pos+=n;
        }
        ok=ok && mbedtls_sha256_finish_ret(&ctx,digest)==0;
        mbedtls_sha256_free(&ctx);
        if(!ok || memcmp(expected,digest,32)) return false;
        esp_err_t result=esp_ota_end(handle); handle=0;
        return result==ESP_OK; // image/chip validation, without changing boot selection
    }
    bool activate() override { return partition && esp_ota_set_boot_partition(partition)==ESP_OK; }
    void abort() override { if(handle) esp_ota_abort(handle); handle=0; partition=nullptr; }
} storage;
Session session(storage);

int nibble(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
size_t fromHex(const char* text,uint8_t* out,size_t capacity) {
    size_t n=strlen(text); if(n%2 || n/2>capacity) return 0;
    for(size_t i=0;i<n/2;i++) {
        int a=nibble(text[2*i]),b=nibble(text[2*i+1]); if(a<0||b<0) return 0;
        out[i]=uint8_t(16*a+b);
    }
    return n/2;
}
void printHex(const uint8_t* p,size_t n) { for(size_t i=0;i<n;i++) Serial.printf("%02x",p[i]); }
void mac(const uint8_t* frame,size_t n,uint8_t tag[32]) {
    // Bind the signature to the recipient as well as every transmitted byte.
    uint8_t message[300]; size_t l=target.length();
    message[0]=uint8_t(l); memcpy(message+1,target.c_str(),l); memcpy(message+1+l,frame,n);
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),secret,32,message,1+l+n,tag);
}
bool authenticated(const uint8_t* p,size_t n) {
    if(!armed || n<51 || n>255 || memcmp(p,"\x3c\xff\x7e\x01",4) || memcmp(p+5,nonce,8)) return false;
    size_t body=(size_t(p[17])<<8)|p[18]; if(n!=51+body) return false;
    uint8_t address[8]{};memcpy(address,target.c_str(),target.length());
    if(memcmp(p+27,address,8)) return false;
    uint8_t tag[32]; mac(p,n-16,tag); uint8_t difference=0;
    for(int i=0;i<16;i++) difference|=tag[i]^p[n-16+i];
    return difference==0;
}
void receiveMode() { received=false; radio.startReceive(); }
void reply(const uint8_t* request,Status status) {
    uint8_t frame[57]{};
    memcpy(frame,"\x3c\xff\x7e\x01",4); frame[4]=ACK;
    memcpy(frame+5,nonce,8); memcpy(frame+13,request+13,4); frame[18]=6;
    memcpy(frame+19,target.c_str(),target.length());memcpy(frame+27,request+19,8);
    frame[35]=request[4]; frame[36]=status; put32(frame+37,session.written);
    uint8_t tag[32];mac(frame,41,tag);memcpy(frame+41,tag,16);
    radio.transmit(frame,sizeof frame); receiveMode();
}
void serialCommand(char* line) {
    if(!strcmp(line,"INFO")) {
        const auto* running=esp_ota_get_running_partition();
        const auto* boot=esp_ota_get_boot_partition();
        Serial.printf("INFO build=%s running=%s boot=%s capacity=%lu state=%d offset=%lu\n",OTA_LABEL,
            running->label,boot->label,(unsigned long)storage.capacity(),int(session.state),(unsigned long)session.written);
    } else if(OTA_RECEIVER && !strncmp(line,"ARM ",4)) {
        char* separator=strchr(line+4,' ');
        if(!separator) return;
        *separator=0;
        if(!strlen(line+4) || strlen(line+4)>8 || fromHex(separator+1,secret,32)!=32) { Serial.println("ERROR arm"); return; }
        session.cancel(); target=line+4;
        esp_fill_random(nonce,8); armedAt=lastPacket=millis(); armed=true;
        Serial.print("ARMED ");printHex(nonce,8);Serial.println();
    } else if(!strcmp(line,"RESET")) {
        Serial.println("RESETTING");Serial.flush();ESP.restart();
    } else if(!strcmp(line,"DISARM")) {
        session.cancel();armed=false;memset(secret,0,32);Serial.println("DISARMED");
    } else if(!OTA_RECEIVER && !strncmp(line,"TX ",3)) {
        uint8_t frame[255];size_t n=fromHex(line+3,frame,sizeof frame);
        // Bridge only carries this bench's private frame type.
        if(n>=51 && !memcmp(frame,"\x3c\xff\x7e\x01",4)) {
            int status=radio.transmit(frame,n);receiveMode();Serial.printf("TXDONE %d\n",status);
        } else Serial.println("ERROR frame");
    }
}
void setup() {
    Serial.begin(115200); delay(400);
    SPI.begin(5,19,27,18);
    int status=radio.begin(434.5,500.0,7,5,0x12,2,8,0);
    if(status!=RADIOLIB_ERR_NONE) {Serial.printf("RADIO_ERROR %d\n",status);while(true) delay(1000);}
    radio.setCRC(true);radio.setDio0Action(onReceive,RISING);receiveMode();
    Serial.printf("BOOT build=%s role=%s freq=434.5 bw=500 sf=7 cr=4/5 power=2dBm\n",OTA_LABEL,OTA_RECEIVER?"receiver":"bridge");
    char info[]="INFO";serialCommand(info);
}
void loop() {
    while(Serial.available()) {
        char c=char(Serial.read());
        if(c=='\n') {input[inputSize]=0;serialCommand(input);inputSize=0;}
        else if(c!='\r') {if(inputSize+1<sizeof input) input[inputSize++]=c;else inputSize=0;}
    }
    if(armed && (uint32_t(millis()-armedAt)>2700000U || (session.state!=IDLE && uint32_t(millis()-lastPacket)>60000U))) {
        session.cancel();armed=false;memset(secret,0,32);Serial.println("TIMEOUT disarmed");
    }
    if(received) {
        received=false;size_t n=radio.getPacketLength();uint8_t frame[255];
        int status=n<=sizeof frame?radio.readData(frame,n):RADIOLIB_ERR_PACKET_TOO_LONG;
        if(status==RADIOLIB_ERR_NONE && !OTA_RECEIVER) {Serial.print("RX ");printHex(frame,n);Serial.println();}
        if(status==RADIOLIB_ERR_NONE && OTA_RECEIVER && authenticated(frame,n) && frame[4]!=ACK) {
            lastPacket=millis();auto result=session.accept(frame[4],get32(frame+13),frame+35,n-51);
            reply(frame,result);
            if(frame[4]!=DATA || result!=labota::OK) Serial.printf("OTA op=%u status=%u offset=%lu\n",frame[4],result,(unsigned long)session.written);
            if(result==labota::OK && session.state==ACTIVATING) {Serial.println("ACTIVATING");Serial.flush();delay(300);ESP.restart();}
        } else receiveMode();
    }
    delay(1);
}
