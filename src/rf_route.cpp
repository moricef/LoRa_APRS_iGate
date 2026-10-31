#include "rf_route.h"
#ifdef ENABLE_RF_ROUTE
#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <atomic>
#include "configuration.h"
#include "lora_utils.h"
#include "rf_ota.h"
#include "profile_lease.h"
#include "session.h"
#include "route_frame.h"
#include "page_cache.h"
#include "relay_diagnostics.h"
extern Configuration Config;
namespace {
constexpr uint8_t ROLE_OFF=0,ROLE_RELAY=1,ROLE_TARGET=2;
uint8_t role=ROLE_OFF,key[32]{};char controller[9]{},target[9]{},via[9]{};
std::atomic<bool> paused{false};std::atomic<int> reportedProfile{0};
std::atomic<bool> active{false};std::atomic<uint32_t> rebootAt{0};
uint32_t started=0,lastPacket=0;bool replyContext=false;
uint32_t bootId=0;
labota::ProfileLease profile;
uint8_t expectedNonce[8]{};uint32_t expectedOffset=0;uint8_t expectedOp=0;
labota::FastProfileRequest proposedFast;
std::atomic<uint32_t> profileRejections{0};
labota::PageCache cache;
#ifdef RFOTA_ROUTE_DIAGNOSTICS
routeota::RelayFault fault;
uint32_t faultMode=0,faultBase=0,faultValue=0;
uint32_t faultArms=0,faultAttempts=0,faultStorageErrors=0;
void executeFault(uint32_t value) {
    // Durable witness distinguishes the intended injection from an unrelated reset.
    Preferences p;
    ++faultAttempts;
    if(!p.begin("ota-diag",false)) {++faultStorageErrors;fault.clear();return;}
    uint32_t witness[3]={fault.mode,fault.base,value};
    bool saved=p.putBytes("fault",witness,sizeof witness)==sizeof witness;p.end();
    fault.clear();
    if(saved)ESP.restart();else ++faultStorageErrors;
}
#endif
uint32_t queuedBlocks=0,queryMask=0;
uint8_t queuedQuery[55]{};size_t queuedQuerySize=0;
uint8_t repairRound=0;
uint32_t knownBase=0,knownBits=0,nextCacheSend=0;
uint8_t knownNonce[8]{};
std::atomic<uint32_t> cacheHits{0},cachedBlocks{0};
std::atomic<uint32_t> localRepairBlocks{0};
std::atomic<uint32_t> requests{0},responses{0},badTags{0},wrongPhase{0},txErrors{0};
bool address(const uint8_t* p,const char* s) {
    uint8_t b[8]{};size_t n=strlen(s);if(!n||n>8)return false;memcpy(b,s,n);return !memcmp(b,p,8);
}
bool decodeKey(const char* encoded,uint8_t* out) {
    if(strlen(encoded)!=43)return false;
    char b[45]{};for(int i=0;i<43;i++)b[i]=encoded[i]=='-'?'+':encoded[i]=='_'?'/':encoded[i];
    b[43]='=';size_t n=0;
    return mbedtls_base64_decode(out,32,&n,reinterpret_cast<uint8_t*>(b),44)==0 && n==32;
}
bool tag(const uint8_t* f,size_t n,uint8_t digest[32]) {
    constexpr char domain[]="LORA-APRS-OTA-HOP1";uint8_t message[280];
    if(n>239)return false;
    memcpy(message,domain,sizeof(domain)-1);memcpy(message+sizeof(domain)-1,f,n);
    return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),key,32,message,n+sizeof(domain)-1,digest)==0;
}
bool verified(const uint8_t* f,size_t n) {
    uint8_t digest[32];if(!tag(f,n-16,digest))return false;
    uint8_t diff=0;for(int i=0;i<16;i++)diff|=digest[i]^f[n-16+i];return !diff;
}
size_t encode(uint8_t phase,const uint8_t* inner,size_t n,uint8_t* out) {
    uint8_t source[8]{};
    const char* hop=(phase==routeota::CONTROLLER_RELAY)?controller:(phase==routeota::TARGET_RELAY)?target:via;
    memcpy(source,hop,strlen(hop));
    size_t count=routeota::envelope(phase,source,inner,n,out);uint8_t digest[32];
    if(!count||!tag(out,count,digest))return 0;
    memcpy(out+count,digest,16);return count+16;
}
void stop() {
#ifdef RFOTA_ROUTE_DIAGNOSTICS
    fault.clear();
#endif
    active=false;
    if(profile.mode!=labota::ProfileLease::NORMAL)LoRa_Utils::setOtaFastProfile(false);
    profile.reset();reportedProfile=0;expectedOp=0;
    queuedBlocks=0;queuedQuerySize=0;cache.clear();
    knownBits=0;memset(knownNonce,0,8);
}
bool save(uint8_t mode,const char* src,const char* dst,const char* relay,const char* encoded) {
    if(!src||!dst||!relay||!strlen(src)||!strlen(dst)||!strlen(relay)||strlen(src)>8||strlen(dst)>8||strlen(relay)>8)return false;
    uint8_t k[32];if(!decodeKey(encoded,k))return false;
    Preferences p;if(!p.begin("ota-route",false))return false;
    bool ok=p.putString("controller",src)==strlen(src) && p.putString("target",dst)==strlen(dst) &&
        p.putString("via",relay)==strlen(relay) && p.putBytes("key",k,32)==32 && p.putUChar("role",mode)==1;
    p.end();memset(k,0,sizeof k);return ok;
}
bool authenticated(AsyncWebServerRequest* r) {
    if(Config.webadmin.active) {
        if(r->authenticate(Config.webadmin.username.c_str(),Config.webadmin.password.c_str()))return true;
    } else if(Config.ota.username.length() && Config.ota.password.length()) {
        if(r->authenticate(Config.ota.username.c_str(),Config.ota.password.c_str()))return true;
    } else if(Config.wifiAutoAP.password.length()>=8) {
        if(r->authenticate("ota-lab",Config.wifiAutoAP.password.c_str()))return true;
    }
    r->requestAuthentication();return false;
}
}
namespace RF_ROUTE {
void recoverConfiguration() {
    if(SPIFFS.begin(false) && !SPIFFS.exists("/igate_conf.json") && SPIFFS.exists("/igate_conf.ota-previous"))
        SPIFFS.rename("/igate_conf.ota-previous","/igate_conf.json");
}
void begin() {
    bootId=esp_random();
#ifdef RFOTA_ROUTE_DIAGNOSTICS
    Preferences diag;
    if(diag.begin("ota-diag",true)) {
        uint32_t witness[3]{};
        if(diag.getBytes("fault",witness,sizeof witness)==sizeof witness) {
            faultMode=witness[0];faultBase=witness[1];faultValue=witness[2];
        }
        diag.end();
    }
#endif
    Preferences p;if(!p.begin("ota-route",true))return;
    uint8_t r=p.getUChar("role",0);String src=p.getString("controller",""),dst=p.getString("target",""),hop=p.getString("via","");
    if((r==ROLE_RELAY||r==ROLE_TARGET) && src.length() && src.length()<=8 && dst.length() && dst.length()<=8 && hop.length() && hop.length()<=8 && p.getBytes("key",key,32)==32) {
        strcpy(controller,src.c_str());strcpy(target,dst.c_str());strcpy(via,hop.c_str());role=r;
    }
    p.end();
    Serial.printf("[RFROUTE] role=%u controller=%s target=%s\n",role,controller,target);
}
bool busy() {return role==ROLE_RELAY && active;}
bool requiresRelay() {return role==ROLE_TARGET;}
bool routedReply() {return replyContext;}
size_t wrapReply(const uint8_t* f,size_t n,uint8_t* out) {return replyContext?encode(routeota::TARGET_RELAY,f,n,out):0;}
bool configureTarget(const char* src,const char* relay,const char* encoded) {return save(ROLE_TARGET,src,Config.callsign.c_str(),relay,encoded);}
void tick() {
    uint32_t now=millis(),reset=rebootAt.load();
    if(reset && int32_t(now-reset)>=0)ESP.restart();
    if(role!=ROLE_RELAY)return;
    if(active && (uint32_t(now-lastPacket)>60000U || uint32_t(now-started)>7200000U))stop();
    auto previous=profile.mode;auto transition=profile.poll(now);
    if(transition==labota::ProfileLease::ENTER_FAST) {
        if(!LoRa_Utils::setOtaFastProfile(true))stop();
    } else if(transition==labota::ProfileLease::RESTORE_NORMAL) {
        LoRa_Utils::setOtaFastProfile(false);
        if(previous==labota::ProfileLease::FAST)stop();
    }
    reportedProfile=int(profile.mode);
    // Controller uploads a burst while this relay only listens. Once queried,
    // the relay owns the channel, forwards cached blocks, then asks for a bitmap.
    if(active && queuedQuerySize && int32_t(now-nextCacheSend)>=0) {
        uint8_t out[255];size_t count=0;
        if(queuedBlocks) {
            unsigned index=__builtin_ctz(queuedBlocks);queuedBlocks&=~(uint32_t(1)<<index);
            count=encode(routeota::RELAY_TARGET,cache.frames[index],cache.lengths[index],out);
            ++cacheHits;
        } else {
            count=encode(routeota::RELAY_TARGET,queuedQuery,queuedQuerySize,out);
            queuedQuerySize=0;
        }
        bool sent=count && LoRa_Utils::sendOtaFrame(out,count);
        if(!sent)++txErrors;else ++requests;
#ifdef RFOTA_ROUTE_DIAGNOSTICS
        if(sent && queuedBlocks && fault.afterBlock(expectedOffset,expectedNonce))executeFault(fault.forwarded);
#endif
        lastPacket=millis();profile.activity(lastPacket);
        nextCacheSend=millis()+(queuedBlocks?5:80); // allow the target's page commit before the query
    }
}
bool handle(const uint8_t* f,size_t n) {
    if(n<3||memcmp(f,"\x3c\xff\x7d",3))return false;
    if(!role)return true;
    routeota::View v;
    if(!routeota::parse(f,n,v)||!verified(f,n)) {++badTags;return true;}
    const uint8_t* inner=v.inner;
    if(role==ROLE_RELAY && paused) {++wrongPhase;return true;}
    if(role==ROLE_TARGET) {
        if(v.phase!=routeota::RELAY_TARGET || !address(v.hopSource,via) || !address(inner+19,controller) || !address(inner+27,target) || inner[4]==128) {++wrongPhase;return true;}
        ++requests;replyContext=true;RF_OTA::handle(inner,v.size);replyContext=false;return true;
    }
    if(v.phase==routeota::CONTROLLER_RELAY && address(v.hopSource,controller) && address(inner+19,controller) && address(inner+27,target) && inner[4]!=128) {
        if(inner[4]==routeota::TEST_RELAY_FAULT) {
#ifdef RFOTA_ROUTE_DIAGNOSTICS
            if(fault.arm(inner,v.size,active.load(),expectedNonce))++faultArms;else ++wrongPhase;
#else
            ++wrongPhase;
#endif
            return true;
        }
        if(inner[4]==routeota::TEST_RELAY_RESTART) {
#if defined(RFOTA_ROUTE_DIAGNOSTICS)
            if(routeota::restartMatchesSession(inner,v.size,active.load(),expectedNonce))
                rebootAt=millis()+100;
            else ++wrongPhase;
#else
            ++wrongPhase;
#endif
            return true; // diagnostic is local to the relay, never forwarded
        }
        if(inner[4]==11) {
            labota::FastProfileRequest candidate;
            bool accepted=labota::FastProfileRequest::decode(inner+35,v.size-51,candidate);
            accepted=accepted && (!candidate.custom || LoRa_Utils::otaProfileSupported(candidate.radio));
            if(accepted && profile.mode==labota::ProfileLease::WAIT_FAST)accepted=candidate==proposedFast;
            else if(accepted)accepted=profile.mode==labota::ProfileLease::NORMAL &&
                LoRa_Utils::configureOtaFastProfile(candidate.radio,candidate.custom);
            if(!accepted) {++profileRejections;return true;}
            proposedFast=candidate;
        }
        if(!active) {active=true;started=millis();}
        lastPacket=millis();profile.activity(lastPacket);
        expectedOp=inner[4];expectedOffset=labota::get32(inner+13);memcpy(expectedNonce,inner+5,8);
#ifdef RFOTA_ROUTE_DIAGNOSTICS
        if(expectedOp==6 || expectedOp==labota::WINDOW_START)fault.clear();
#endif
        if(expectedOp==6 || expectedOp==labota::WINDOW_START)cache.clear();
        if(expectedOp==labota::WINDOW_DATA) {
            if(cache.put(inner,v.size))++cachedBlocks;else ++wrongPhase;
            return true;
        }
        if(expectedOp==labota::WINDOW_QUERY && v.size==55) {
            queryMask=labota::get32(inner+35);repairRound=0;
            if(knownBase!=expectedOffset || memcmp(knownNonce,expectedNonce,8)) {
                knownBase=expectedOffset;memcpy(knownNonce,expectedNonce,8);knownBits=0;
            }
            memcpy(queuedQuery,inner,v.size);queuedQuerySize=v.size;
            queuedBlocks=cache.available(expectedOffset,expectedNonce)&queryMask&~knownBits;
            nextCacheSend=millis()+10;
            return true;
        }
        uint8_t out[255];size_t count=encode(routeota::RELAY_TARGET,inner,v.size,out);
        if(!count||!LoRa_Utils::sendOtaFrame(out,count))++txErrors;else ++requests;
    } else if(v.phase==routeota::TARGET_RELAY && address(v.hopSource,target) && active && address(inner+19,target) && address(inner+27,controller) &&
              inner[4]==128 && v.size>=57 && inner[35]==expectedOp && labota::get32(inner+13)==expectedOffset &&
              (expectedOp==6 || !memcmp(inner+5,expectedNonce,8))) {
        lastPacket=millis();profile.activity(lastPacket);
        if(expectedOp==labota::WINDOW_QUERY && v.size==61 && inner[36]==0) {
            knownBits=labota::get32(inner+41);
#ifdef RFOTA_ROUTE_DIAGNOSTICS
            uint32_t written=labota::get32(inner+37);
            if(fault.afterBitmap(expectedOffset,expectedNonce,written,knownBits)) {
                executeFault(written);
                return true; // never forward this committed-page ACK to the controller
            }
#endif
            uint32_t missing=queryMask&~knownBits;
            uint32_t local=missing&cache.available(expectedOffset,expectedNonce);
            if(local && repairRound<3) {
                localRepairBlocks+=__builtin_popcount(local);
                ++repairRound;queuedBlocks=local;queuedQuerySize=55;nextCacheSend=millis()+10;
                return true; // repair locally before involving the controller
            }
        }
        uint8_t out[255];size_t count=encode(routeota::RELAY_CONTROLLER,inner,v.size,out);
        bool sent=count && LoRa_Utils::sendOtaFrame(out,count);if(!sent)++txErrors;else ++responses;
        if(sent && inner[36]==0) {
            if(expectedOp==11)profile.prepare(millis(),proposedFast.lease);
            if(expectedOp==12)profile.requestReturn(millis());
            if(expectedOp==5)stop();
        }
    } else ++wrongPhase;
    return true;
}
void setupWeb(AsyncWebServer* server) {
#if defined(RFOTA_DIAGNOSTICS) || defined(RFOTA_ROUTE_DIAGNOSTICS)
    server->on("/lab/ota/route",HTTP_GET,[](AsyncWebServerRequest* r){
        if(!authenticated(r))return;
        JsonDocument d;d["role"]=role;d["active"]=active.load();d["controller"]=controller;d["target"]=target;d["via"]=via;
        d["requests_forwarded"]=requests.load();d["responses_forwarded"]=responses.load();d["bad_tags"]=badTags.load();
        d["wrong_phase"]=wrongPhase.load();d["tx_errors"]=txErrors.load();d["profile"]=reportedProfile.load();d["paused"]=paused.load();
        d["cached_blocks"]=cachedBlocks.load();d["cache_sends"]=cacheHits.load();
        d["local_repair_blocks"]=localRepairBlocks.load();
        d["profile_rejections"]=profileRejections.load();
        d["boot_id"]=bootId;d["uptime_ms"]=millis();
#ifdef RFOTA_ROUTE_DIAGNOSTICS
        d["fault_mode"]=faultMode;d["fault_base"]=faultBase;d["fault_value"]=faultValue;
        d["armed_fault_mode"]=fault.mode;d["armed_fault_base"]=fault.base;d["fault_forwarded"]=fault.forwarded;
        d["fault_arms"]=faultArms;d["fault_attempts"]=faultAttempts;d["fault_storage_errors"]=faultStorageErrors;
#endif
        esp_app_desc_t desc;
        if(esp_ota_get_partition_description(esp_ota_get_running_partition(),&desc)==ESP_OK) {
            char hex[65]{};for(int i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",desc.app_elf_sha256[i]);
            d["elf_sha256"]=hex;
        }
        String body;serializeJson(d,body);r->send(200,"application/json",body);
    });
    server->on("/lab/ota/pause",HTTP_POST,[](AsyncWebServerRequest* r){
        if(!authenticated(r))return;
        if(busy()) {r->send(409);return;}
        if(!r->hasParam("paused",true)) {r->send(400);return;}
        paused=r->getParam("paused",true)->value()=="1";
        r->send(200,"text/plain",paused?"Paused":"Forwarding enabled");
    });
    server->on("/lab/ota/config-backup",HTTP_GET,[](AsyncWebServerRequest* r){
        if(!authenticated(r))return;
        r->send(SPIFFS,"/igate_conf.json","application/json");
    });
    server->on("/lab/ota/route",HTTP_POST,[](AsyncWebServerRequest* r){
        if(!authenticated(r))return;
        if(busy()||RF_OTA::busy()) {r->send(409,"text/plain","OTA session active");return;}
        if(!r->hasParam("controller",true)||!r->hasParam("target",true)||!r->hasParam("key",true)) {r->send(400);return;}
        auto src=r->getParam("controller",true)->value(),dst=r->getParam("target",true)->value(),k=r->getParam("key",true)->value();
        if(!save(ROLE_RELAY,src.c_str(),dst.c_str(),Config.callsign.c_str(),k.c_str())) {r->send(400);return;}
        if(r->hasParam("bench_profile",true) && r->getParam("bench_profile",true)->value()=="1") {
            Config.loramodule.rxFreq=Config.loramodule.txFreq=434100000;
            Config.loramodule.rxSignalBandwidth=Config.loramodule.txSignalBandwidth=125000;
            Config.loramodule.rxSpreadingFactor=Config.loramodule.txSpreadingFactor=12;
            Config.loramodule.rxCodingRate4=Config.loramodule.txCodingRate4=5;Config.loramodule.power=2;Config.digi.ecoMode=0;
            if(!Config.writeFile()) {r->send(500);return;}
        }
        r->send(200,"text/plain","Relay configured; rebooting");rebootAt=millis()+700;
    });
    server->on("/lab/ota/config-restore",HTTP_POST,[](AsyncWebServerRequest* r){
        if(!authenticated(r))return;
        if(busy()||RF_OTA::busy()) {r->send(409);return;}
        if(!r->hasParam("json",true)) {r->send(400);return;}
        String raw=r->getParam("json",true)->value();JsonDocument d;
        if(deserializeJson(d,raw)||String(d["callsign"]|"")!=Config.callsign) {r->send(400);return;}
        File f=SPIFFS.open("/igate_conf.restore","w");
        bool ok=f && f.print(raw)==raw.length();f.close();
        if(!ok) {r->send(500);return;}
        if(SPIFFS.exists("/igate_conf.ota-previous"))SPIFFS.remove("/igate_conf.ota-previous");
        if(!SPIFFS.rename("/igate_conf.json","/igate_conf.ota-previous")) {r->send(500);return;}
        if(!SPIFFS.rename("/igate_conf.restore","/igate_conf.json")) {
            SPIFFS.rename("/igate_conf.ota-previous","/igate_conf.json");r->send(500);return;
        }
        Preferences p;if(p.begin("ota-route",false)) {p.clear();p.end();}
        r->send(200,"text/plain","Configuration restored; rebooting");rebootAt=millis()+700;
    });
#endif
}
}
#endif
