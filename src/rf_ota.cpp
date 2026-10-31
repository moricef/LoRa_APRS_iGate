#include "rf_ota.h"
#include "rf_route.h"
#ifdef ENABLE_RF_OTA
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include "network_manager.h"
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <atomic>
#include "configuration.h"
#include "remote_auth.h"
#include "lora_utils.h"
#include "flash_store.h"
#include "profile_lease.h"
#include "ota_aprs_control.h"
#include "station_utils.h"
#include "window_flash_store.h"

extern Configuration Config;
extern NetworkManager* networkManager;
extern String versionNumber;
extern bool isUpdatingOTA;

// Prevent Arduino from confirming an OTA image before its radio is tested.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
using namespace labota;
constexpr uint8_t HELLO=6,CONFIRM=7,GET_STATUS=8,TEST_FAIL_ONCE=9,REACTIVATE=10,PREPARE_FAST=11,RETURN_NORMAL=12,TEST_REBOOT=13,NETWORK_STATUS=14;
std::atomic<bool> pending{false},armed{false};
bool radioReady=false,watchdog=false;
uint32_t bootAt=0,armedAt=0,lastPacket=0,rebootAt=0,helloCounter=0;
uint8_t nonce[8]{},peer[8]{};
char serialInput[160];size_t serialLength=0;

class CandidateStore : public FlashStore {
public:
    uint8_t digest[32]{};
    bool finish(const uint8_t sha[32]) override {
        if(!FlashStore::finish(sha))return false;
        memcpy(digest,sha,32);return true;
    }
    bool activate() override {
        Preferences p;
        if(!p.begin("rf-ota",false))return false;
        bool ok=p.putUInt("candidate",partition->address)==sizeof(uint32_t) &&
            p.putUInt("size",bytes)==sizeof(uint32_t) && p.putBytes("sha",digest,32)==32;
        p.end();return ok && FlashStore::activate();
    }
} storage;
Session session(storage);
WindowFlashStore pageStorage;
WindowSession pages(pageStorage);
#ifdef RFOTA_DIAGNOSTICS
uint32_t dropWindowOnce=0;
#endif
ProfileLease profile;
FastProfileRequest proposedFast;
RadioProfile aprsSelectedProfile;
bool aprsProfileSelected=false;

bool equalTag(const uint8_t* f,size_t n) {
    uint8_t digest[32];if(!REMOTE_AUTH::otaTag(f,n-16,digest))return false;
    uint8_t difference=0;for(int i=0;i<16;i++)difference|=digest[i]^f[n-16+i];
    return difference==0;
}
bool address(const uint8_t* bytes,const String& value) {
    if(value.isEmpty()||value.length()>8)return false;
    uint8_t padded[8]{};memcpy(padded,value.c_str(),value.length());
    return !memcmp(bytes,padded,8);
}
bool runningManifest(uint8_t sha[32],uint32_t& bytes) {
    Preferences p;if(!p.begin("rf-ota",true))return false;
    bool ok=p.getUInt("candidate",0)==esp_ota_get_running_partition()->address &&
        p.getBytes("sha",sha,32)==32;
    bytes=p.getUInt("size",0);p.end();return ok && bytes;
}
void reply(const uint8_t* request,Status result) {
    uint8_t frame[160]{};memcpy(frame,"\x3c\xff\x7e\x02",4);frame[4]=ACK;
    memcpy(frame+5,nonce,8);memcpy(frame+13,request+13,4);
    memcpy(frame+19,Config.callsign.c_str(),Config.callsign.length());memcpy(frame+27,request+19,8);
    bool windowOp=request[4]>=WINDOW_START && request[4]<=WINDOW_ACTIVATE;
#ifdef RFOTA_DIAGNOSTICS
    windowOp=windowOp || request[4]==24;
#endif
    frame[35]=request[4];frame[36]=result;put32(frame+37,windowOp?pages.written:session.written);
#ifdef RFOTA_DIAGNOSTICS
    if(request[4]==24) {
        frame[41]=pageStorage.cutPaused?1:0;put32(frame+42,pageStorage.cutOffset);
        frame[46]=uint8_t(pageStorage.cutBytes>>8);frame[47]=uint8_t(pageStorage.cutBytes);
        frame[18]=13;uint8_t digest[32];
        if(REMOTE_AUTH::otaTag(frame,48,digest)) {
            memcpy(frame+48,digest,16);LoRa_Utils::sendOtaFrame(frame,64);
        }
        return;
    }
#endif
    if(request[4]==WINDOW_START || request[4]==WINDOW_QUERY) {
        put32(frame+41,pages.bitmap(get32(request+13)));frame[18]=10;uint8_t digest[32];
        if(REMOTE_AUTH::otaTag(frame,45,digest)) {
            memcpy(frame+45,digest,16);LoRa_Utils::sendOtaFrame(frame,61);
        }
        return;
    }
    if(request[4]==NETWORK_STATUS) {
        frame[41]=uint8_t(WiFi.getMode());frame[42]=networkManager->isConnected()?1:0;
        frame[43]=networkManager->hasActiveInterface()?1:0;frame[18]=9;
        uint8_t digest[32];
        if(REMOTE_AUTH::otaTag(frame,44,digest)) {
            memcpy(frame+44,digest,16);LoRa_Utils::sendOtaFrame(frame,60);
        }
        return;
    }
    if(request[4]==DATA) {
        frame[18]=6;uint8_t digest[32];
        if(REMOTE_AUTH::otaTag(frame,41,digest)) {
            memcpy(frame+41,digest,16);LoRa_Utils::sendOtaFrame(frame,57);
        }
        return;
    }
    frame[41]=pending?1:0;
    put32(frame+42,esp_ota_get_running_partition()->address);
    put32(frame+46,esp_ota_get_boot_partition()->address);
    size_t vl=std::min(size_t(versionNumber.length()),size_t(40));frame[50]=uint8_t(vl);
    memcpy(frame+51,versionNumber.c_str(),vl);
    uint32_t bytes=0;runningManifest(frame+51+vl,bytes);
    size_t len=51+vl+32;size_t body=len-35;frame[17]=uint8_t(body>>8);frame[18]=uint8_t(body);
    uint8_t digest[32];if(!REMOTE_AUTH::otaTag(frame,len,digest))return;
    memcpy(frame+len,digest,16);LoRa_Utils::sendOtaFrame(frame,len+16);
}
void cancel() {
    session.cancel();armed=false;memset(nonce,0,8);
    pages.suspend(); // retain the durable checkpoint for a fresh authenticated session
    if(profile.mode!=ProfileLease::NORMAL)LoRa_Utils::setOtaFastProfile(false);
    profile.reset();
}
void rollback() {
    Serial.println("[RFOTA] Unconfirmed application: rollback");Serial.flush();
    esp_ota_mark_app_invalid_rollback_and_reboot();
    // If no rollback slot is available, reset rather than silently confirm.
    ESP.restart();
}
void diagnostics() {
#ifdef RFOTA_DIAGNOSTICS
    while(Serial.available()) {
        char c=char(Serial.read());
        if(c=='\n') {
            serialInput[serialLength]=0;
            if(!strncmp(serialInput,"OTAPROVISION ",13)) {
                char* key=strchr(serialInput+13,' ');
                if(key) {
                    *key++=0;
                    if(REMOTE_AUTH::setSecret(String(key))) {
                        Config.remoteManagement.authController=serialInput+13;
                        Serial.println(Config.writeFile()?"OTAPROVISIONED":"OTAPROVISION_ERROR");
                    }
                }
            } else if(!strcmp(serialInput,"OTATESTPROFILE")) {
                Config.loramodule.rxFreq=Config.loramodule.txFreq=434500000;
                Config.loramodule.rxSignalBandwidth=Config.loramodule.txSignalBandwidth=500000;
                Config.loramodule.rxSpreadingFactor=Config.loramodule.txSpreadingFactor=7;
                Config.loramodule.rxCodingRate4=Config.loramodule.txCodingRate4=5;
                Config.loramodule.power=2;Config.digi.ecoMode=0;
                Serial.println(Config.writeFile()?"OTAPROFILE_SAVED":"OTAPROFILE_ERROR");
            } else if(!strcmp(serialInput,"OTABASEPROFILE")) {
                Config.loramodule.rxFreq=Config.loramodule.txFreq=434100000;
                Config.loramodule.rxSignalBandwidth=Config.loramodule.txSignalBandwidth=125000;
                Config.loramodule.rxSpreadingFactor=Config.loramodule.txSpreadingFactor=12;
                Config.loramodule.rxCodingRate4=Config.loramodule.txCodingRate4=5;
                Config.loramodule.power=2;Config.digi.ecoMode=0;
                Serial.println(Config.writeFile()?"OTABASE_SAVED":"OTABASE_ERROR");
            } else if(!strcmp(serialInput,"OTAOFFLINE")) {
                Config.wifiAPs.clear();Config.wifiAutoAP.enabled=false;
                bool saved=Config.writeFile();
                networkManager->disconnectWiFi();networkManager->disableAP();
                Serial.println(saved?"OTAOFFLINE_SAVED":"OTAOFFLINE_ERROR");
            } else if(!strncmp(serialInput,"OTAROUTE ",9)) {
                char* encoded=strchr(serialInput+9,' ');
                if(encoded) {*encoded++=0;Serial.println(RF_ROUTE::configureTarget(Config.remoteManagement.authController.c_str(),serialInput+9,encoded)?"OTAROUTE_SAVED":"OTAROUTE_ERROR");}
            } else if(!strcmp(serialInput,"OTARESET")) {
                Serial.flush();ESP.restart();
            }
            serialLength=0;memset(serialInput,0,sizeof serialInput);
        } else if(c!='\r' && serialLength+1<sizeof serialInput) serialInput[serialLength++]=c;
    }
#endif
}
}
namespace RF_OTA {
void earlyBoot() {
    esp_ota_img_states_t state;
    pending=esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY;
    bootAt=millis();
    if(pending) {
        // A crash/hang before the main loop must also reach the bootloader.
        esp_task_wdt_init(60,true);watchdog=esp_task_wdt_add(nullptr)==ESP_OK;
        Serial.println("[RFOTA] Pending radio confirmation");
        Preferences p;
        if(p.begin("rf-ota",false)) {
            bool fail=p.getBool("fail-once",false);
            if(fail) p.remove("fail-once");
            p.end();
            if(fail) {Serial.println("[RFOTA] Injected unconfirmed-boot failure");Serial.flush();delay(3000);ESP.restart();}
        }
    }
}
void begin() {
    radioReady=true;
    Serial.printf("[RFOTA] Ready version=%s running=%s pending=%u\n",versionNumber.c_str(),esp_ota_get_running_partition()->label,unsigned(pending.load()));
}
bool busy() {return armed||pending||profile.mode!=ProfileLease::NORMAL;}

bool handleAprsControl(const String& packet) {
    // Only direct RF APRS messages, addressed to this node, from the configured
    // controller. Other traffic is left to the ordinary APRS handlers.
    int gt=packet.indexOf('>'), info=packet.indexOf(':');
    if(gt<1 || info<gt || packet.length()<unsigned(info+12) ||
       packet[info+1]!=':' || packet[info+11]!=':')return false;
    String target=packet.substring(info+2,info+11);target.trim();
    String local=Config.tacticalCallsign.isEmpty()?Config.callsign:Config.tacticalCallsign;
    if(target!=local)return false;
    String envelope=packet.substring(info+12);
    int brace=envelope.indexOf('{');String messageId;
    if(brace>=0) {
        messageId=envelope.substring(brace+1);envelope=envelope.substring(0,brace);
        int suffix=messageId.indexOf('}');if(suffix>=0)messageId=messageId.substring(0,suffix);
        if(messageId.length()>5)messageId="";
        for(unsigned i=0;i<messageId.length();++i)
            if(!((messageId[i]>='0'&&messageId[i]<='9')||
                 (messageId[i]>='A'&&messageId[i]<='Z')||
                 (messageId[i]>='a'&&messageId[i]<='z'))) {messageId="";break;}
    }
    if(!envelope.startsWith("!RC1:"))return false;
    int start=5;
    for(int i=0;i<2;++i) {start=envelope.indexOf(':',start);if(start<0)return false;++start;}
    if(!envelope.substring(start).startsWith("OTA="))return false;
    String controller=packet.substring(0,gt);
    if(controller!=Config.remoteManagement.authController || !Config.loramodule.txActive ||
       Config.digi.ecoMode!=0 || isUpdatingOTA)return true;
    auto auth=REMOTE_AUTH::verifyAndConsume(envelope,controller,
        Config.remoteManagement.authController,Config.callsign);
    auto sendText=[&](const String& text) {
        String to=controller;while(to.length()<9)to+=' ';
        String response=STATION_Utils::localReplyHeader(local,controller,false)+"::"+to+":"+text;
        String wire=String("\x3c\xff\x01")+response;
        return LoRa_Utils::sendOtaFrame(reinterpret_cast<const uint8_t*>(wire.c_str()),wire.length(),true);
    };
    if(auth.status!=REMOTE_AUTH::Status::Accepted && auth.status!=REMOTE_AUTH::Status::Replay) {
        sendText("AUTH FAILED");return true;
    }
    STATION_Utils::learnReturnPath(controller,packet);
    if(!messageId.isEmpty())sendText("ack"+messageId);
    if(auth.status==REMOTE_AUTH::Status::Replay)return true;
    AprsOtaCommand command;String answer;
    bool switchFast=false,returnNormal=false;
    if(!AprsOtaCommand::parse(std::string(auth.command.c_str()),command))answer="OTA BAD COMMAND";
    else if(command.kind==AprsOtaCommand::STATUS) {
        answer="OTA "+String(profile.mode==ProfileLease::FAST?"FAST":"NORMAL")+
            " ARMED="+String(armed?1:0)+" PROFILE="+String(aprsProfileSelected?1:0);
        if(aprsProfileSelected)answer+=" "+String(aprsSelectedProfile.frequencyHz)+","+
            String(aprsSelectedProfile.bandwidthHz)+","+String(aprsSelectedProfile.sf)+","+
            String(aprsSelectedProfile.cr)+","+String(aprsSelectedProfile.power);
    } else if(pending || RF_ROUTE::busy())answer="OTA BUSY";
    else if(command.kind==AprsOtaCommand::NORMAL) {
        answer="OTA RETURN NORMAL";returnNormal=true;
    } else if(armed || profile.mode!=ProfileLease::NORMAL || session.state!=IDLE || pages.state!=IDLE)
        answer="OTA BUSY";
    else if(command.kind==AprsOtaCommand::PROFILE) {
        if(!LoRa_Utils::otaProfileSupported(command.radio))answer="OTA UNSUPPORTED PROFILE";
        else {aprsSelectedProfile=command.radio;aprsProfileSelected=true;answer="OTA PROFILE READY (RAM)";}
    } else if(!aprsProfileSelected)answer="OTA SET PROFILE FIRST";
    else if(!LoRa_Utils::configureOtaFastProfile(aprsSelectedProfile,true))answer="OTA PROFILE ERROR";
    else {answer="OTA FAST IN 2S LEASE="+String(command.lease);switchFast=true;}
    // Transmit the application response on the CURRENT profile before any
    // switch. A failed transmit must never trigger the requested transition.
    bool sent=sendText(answer);
    if(sent && switchFast)profile.prepare(millis(),command.lease);
    if(sent && returnNormal)cancel();
    return true;
}
void tick() {
    if(watchdog)esp_task_wdt_reset();
    if(pending && uint32_t(millis()-bootAt)>180000U)rollback();
    if(rebootAt && int32_t(millis()-rebootAt)>=0) {Serial.flush();ESP.restart();}
    if(armed && (uint32_t(millis()-armedAt)>7200000U || uint32_t(millis()-lastPacket)>60000U)) {
        Serial.println("[RFOTA] Session expired");cancel();
    }
    auto previous=profile.mode;
    auto transition=profile.poll(millis());
    if(transition==ProfileLease::ENTER_FAST) {
        if(!LoRa_Utils::setOtaFastProfile(true))cancel();
        Serial.println("[RFOTA] Temporary fast profile");
    } else if(transition==ProfileLease::RESTORE_NORMAL) {
        LoRa_Utils::setOtaFastProfile(false);
        if(previous==ProfileLease::FAST)cancel(); // inactivity abandons the session
        Serial.println("[RFOTA] Configured radio profile restored");
    }
    diagnostics();
}
bool handle(const uint8_t* f,size_t n) {
    if(n<3||memcmp(f,"\x3c\xff\x7e",3))return false;
    if(n<51||n>255||f[3]!=2||isUpdatingOTA||!Config.loramodule.txActive||Config.digi.ecoMode!=0)return true;
    size_t body=(size_t(f[17])<<8)|f[18];
    if(n!=51+body || !address(f+27,Config.callsign) ||
       !address(f+19,Config.remoteManagement.authController) || !equalTag(f,n))return true;
    uint32_t offset=get32(f+13);uint8_t op=f[4];
    if(op==HELLO) {
        if(body||memcmp(f+5,"\0\0\0\0\0\0\0\0",8)||!offset)return true;
        if(armed) {
            if(offset==helloCounter && !memcmp(peer,f+19,8))reply(f,labota::OK);
            else reply(f,labota::BUSY);
            return true;
        }
        Preferences p;if(!p.begin("rf-ota",false))return true;
        uint32_t previous=p.getUInt("counter",0);
        if(offset<=previous || p.putUInt("counter",offset)!=sizeof(uint32_t)) {p.end();return true;}
        p.end();esp_fill_random(nonce,8);memcpy(peer,f+19,8);helloCounter=offset;
        armed=true;armedAt=lastPacket=millis();reply(f,labota::OK);
        Serial.println("[RFOTA] Authenticated session armed");return true;
    }
    if(!armed||memcmp(f+5,nonce,8)||memcmp(peer,f+19,8))return true;
    lastPacket=millis();profile.activity(lastPacket);Status result=FORMAT;
    if(op==PREPARE_FAST) {
        FastProfileRequest candidate;
        if(pending || session.state!=IDLE || pages.state!=IDLE ||
           !FastProfileRequest::decode(f+35,body,candidate) ||
           (candidate.custom && !LoRa_Utils::otaProfileSupported(candidate.radio)))result=FORMAT;
        else {
            // Schedule only after the ACK has finished transmitting below.
            result=ORDER;
            if(profile.mode==ProfileLease::WAIT_FAST && candidate==proposedFast)result=labota::OK;
            else if(profile.mode==ProfileLease::NORMAL &&
                    LoRa_Utils::configureOtaFastProfile(candidate.radio,candidate.custom)) {
                proposedFast=candidate;result=labota::OK;
            }
        }
    }
    else if(op==RETURN_NORMAL)result=body?FORMAT:labota::OK;
    else if(op==GET_STATUS || op==NETWORK_STATUS)result=body?FORMAT:labota::OK;
    else if(op==CONFIRM) {
        uint8_t expected[32],actual[32];uint32_t bytes=0;
        if(!radioReady || body!=32)result=ORDER;
        else if(!runningManifest(expected,bytes) || memcmp(expected,f+35,32) ||
                !hashPartition(esp_ota_get_running_partition(),bytes,actual) || memcmp(expected,actual,32))result=VERIFY;
        else if(pending && esp_ota_mark_app_valid_cancel_rollback()!=ESP_OK)result=STORAGE;
        else {
            pending=false;if(watchdog)esp_task_wdt_delete(nullptr);watchdog=false;
            result=labota::OK;Serial.println("[RFOTA] Radio-confirmed application validated");
        }
    }
#ifdef RFOTA_DIAGNOSTICS
    else if(op==23 && !pending && !body) {
        result=pages.state==RECEIVING && pages.written==offset && pages.pageSize(offset)==PAGE_BYTES &&
            pageStorage.armPowerCut(offset)?labota::OK:ORDER;
    }
    else if(op==24)result=body?FORMAT:labota::OK;
    else if(op==20 && !pending && body==4) {dropWindowOnce=get32(f+35);result=labota::OK;}
    else if(op==TEST_REBOOT && !pending && body==0) {result=labota::OK;rebootAt=millis()+400;}
    else if(op==TEST_FAIL_ONCE && !pending && (session.state==READY || pages.state==READY) && body==0) {
        Preferences p;
        if(p.begin("rf-ota",false)) {result=p.putBool("fail-once",true)==1?labota::OK:STORAGE;p.end();}
        else result=STORAGE;
    }
#endif
    else if(op==REACTIVATE && !pending && body==32 && session.state==IDLE) {
        Preferences p;uint8_t expected[32],actual[32];uint32_t addr=0,bytes=0;
        bool found=p.begin("rf-ota",true);
        if(found) {addr=p.getUInt("candidate",0);bytes=p.getUInt("size",0);found=p.getBytes("sha",expected,32)==32;p.end();}
        auto part=esp_ota_get_next_update_partition(nullptr);
        if(!found || !part || part->address!=addr || memcmp(expected,f+35,32) ||
           !hashPartition(part,bytes,actual) || memcmp(expected,actual,32))result=VERIFY;
        else if(esp_ota_set_boot_partition(part)!=ESP_OK)result=STORAGE;
        else {result=labota::OK;rebootAt=millis()+400;}
    }
    else if(pending)result=labota::BUSY;
    else if(op==WINDOW_START)result=offset || session.state!=IDLE?ORDER:pages.start(f+35,body);
    else if(op==WINDOW_DATA) {
#ifdef RFOTA_DIAGNOSTICS
        if(pageStorage.cutPaused)return true;
        uint32_t bit=uint32_t(1)<<((offset%PAGE_BYTES)/WINDOW_CHUNK);
        if(dropWindowOnce&bit) {dropWindowOnce&=~bit;return true;}
#endif
        result=pages.data(offset,f+35,body);
    }
    else if(op==WINDOW_QUERY) {
        result=body==4 && pages.state!=IDLE && offset%PAGE_BYTES==0 && offset<pages.manifest.size &&
            get32(f+35)==WindowSession::mask(pages.pageSize(offset))?labota::OK:FORMAT;
#ifdef RFOTA_DIAGNOSTICS
        if(result==labota::OK && pageStorage.cutPaused)result=labota::BUSY;
#endif
    }
    else if(op==WINDOW_FINISH)result=body || offset!=pages.manifest.size?FORMAT:pages.finish();
    else if(op==WINDOW_ACTIVATE)result=body || offset!=pages.manifest.size?FORMAT:pages.activate();
    else if(op==START) {
        WindowManifest saved;uint32_t committed;
        result=pages.state!=IDLE || pageStorage.load(saved,committed)?labota::BUSY:session.accept(op,offset,f+35,body);
    }
    else if(op==ABORT && !body)result=pages.discard();
    else result=session.accept(op,offset,f+35,body);
    // A burst carries no per-block ACK. Its bitmap query reports all holes.
    if(op==WINDOW_DATA)return true;
    reply(f,result);
    if(result==labota::OK && op==PREPARE_FAST)profile.prepare(millis(),proposedFast.lease);
    if(result==labota::OK && op==RETURN_NORMAL)profile.requestReturn(millis());
    if(result==labota::OK && (op==ACTIVATE || op==WINDOW_ACTIVATE))rebootAt=millis()+400;
    if(result==labota::OK && op==ABORT)cancel();
    if(op!=DATA || result!=labota::OK)Serial.printf("[RFOTA] op=%u status=%u offset=%lu\n",op,result,(unsigned long)session.written);
    return true;
}
}
#endif
