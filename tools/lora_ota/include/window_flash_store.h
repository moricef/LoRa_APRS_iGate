#pragma once
#include <Preferences.h>
#include <esp_image_format.h>
#if CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/miniz.h"
#else
#include "esp32/rom/miniz.h"
#endif
#include "flash_store.h"
#include "window_session.h"
#include "page_inflater.h"

namespace labota {
class WindowFlashStore : public WindowStore {
    const esp_partition_t* partition=nullptr;
    // MAGIC: plain stream. MAGIC_CODED adds coding, decoded size and the
    // decoder's durable position (container offset and image offset).
    static constexpr uint32_t MAGIC=0x574f5431,MAGIC_CODED=0x574f5432;
    WindowManifest current;
    PageInflater inflater;
    static size_t inflateRom(void*,uint8_t* out,size_t capacity,const uint8_t* in,size_t n) {
        static tinfl_decompressor decompressor;
        tinfl_init(&decompressor);
        size_t inLength=n,outLength=capacity;
        tinfl_status status=tinfl_decompress(&decompressor,in,&inLength,out,out,&outLength,
            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        return status==TINFL_STATUS_DONE && inLength==n?outLength:SIZE_MAX;
    }
    static bool writeDecoded(void* context,uint32_t offset,const uint8_t* data,size_t n) {
        auto* p=static_cast<const esp_partition_t*>(context);
        return p && offset%PAGE_BYTES==0 && n && n<=PAGE_BYTES && offset<=p->size && PAGE_BYTES<=p->size-offset &&
            esp_partition_erase_range(p,offset,PAGE_BYTES)==ESP_OK && esp_partition_write(p,offset,data,n)==ESP_OK;
    }
    void bindInflater() {inflater.inflate=inflateRom;inflater.write=writeDecoded;inflater.context=const_cast<esp_partition_t*>(partition);}
    bool coded() const {return current.coding==WindowManifest::DEFLATE_PAGES;}
public:
#ifdef RFOTA_DIAGNOSTICS
    bool cutArmed=false,cutPaused=false;
    uint32_t cutOffset=0;
    uint16_t cutBytes=0;
    bool armPowerCut(uint32_t offset) {
        if(cutPaused || offset%PAGE_BYTES)return false;
        cutOffset=offset;cutBytes=0;cutArmed=true;return true;
    }
#endif
    uint32_t capacity() const override {
        auto p=esp_ota_get_next_update_partition(nullptr);return p?p->size:0;
    }
    bool load(WindowManifest& manifest,uint32_t& committed) override {
        Preferences p;if(!p.begin("ota-pages",true))return false;
        uint8_t record[68]{};
        size_t length=p.getBytesLength("checkpoint");
        bool found=(length==52||length==68) && p.getBytes("checkpoint",record,length)==length;p.end();
        partition=esp_ota_get_next_update_partition(nullptr);
        if(!found || !partition || get32(record+4)!=partition->address || get32(record+48)!=BOARD)return false;
        WindowManifest saved;saved.size=get32(record+8);committed=get32(record+12);memcpy(saved.sha,record+16,32);
        if(length==52) {
            if(get32(record)!=MAGIC)return false;
        } else {
            if(get32(record)!=MAGIC_CODED || record[52]!=WindowManifest::DEFLATE_PAGES)return false;
            saved.coding=record[52];saved.imageSize=get32(record+56);
            uint32_t consumed=get32(record+60),out=get32(record+64);
            if(!saved.imageSize || saved.imageSize>partition->size || consumed>saved.size || out>saved.imageSize)return false;
            bindInflater();inflater.reset(saved.imageSize,saved.size,consumed,out);
            committed=inflater.resumeOffset();
        }
        if(!saved.size || saved.size>partition->size)return false;
        current=saved;manifest=saved;return true;
    }
    bool checkpoint(const WindowManifest& manifest,uint32_t committed) override {
        partition=esp_ota_get_next_update_partition(nullptr);
        if(!partition || manifest.size>partition->size || manifest.decodedSize()>partition->size)return false;
        bool fresh=!current.same(manifest) || committed==0;
        current=manifest;
        uint8_t record[68]{};put32(record+4,partition->address);
        put32(record+8,manifest.size);put32(record+12,committed);memcpy(record+16,manifest.sha,32);put32(record+48,BOARD);
        size_t length=52;
        if(coded()) {
            // The session's page count is not the decoder's position: record the latter.
            if(fresh) {bindInflater();inflater.reset(manifest.imageSize,manifest.size);}
            put32(record,MAGIC_CODED);record[52]=manifest.coding;put32(record+56,manifest.imageSize);
            put32(record+60,inflater.consumed);put32(record+64,inflater.out);length=68;
        } else put32(record,MAGIC);
        Preferences p;if(!p.begin("ota-pages",false))return false;
        bool ok=p.putBytes("checkpoint",record,length)==length;p.end();return ok;
    }
    bool page(uint32_t offset,const uint8_t* data,size_t n) override {
#ifdef RFOTA_DIAGNOSTICS
        if(cutPaused)return false;
        if(cutArmed && offset==cutOffset && !coded()) {
            if(!partition || n!=PAGE_BYTES || offset>partition->size || PAGE_BYTES>partition->size-offset ||
               esp_partition_erase_range(partition,offset,PAGE_BYTES)!=ESP_OK)return false;
            if(esp_partition_write(partition,offset,data,PAGE_BYTES/2)!=ESP_OK)return false;
            cutArmed=false;cutPaused=true;cutBytes=PAGE_BYTES/2;
            // Keep a partially written page and the previous durable checkpoint.
            // No more data writes until a reset, or an explicit test abort.
            Serial.printf("[RFOTA] POWER_CUT_READY offset=%lu programmed=%u\n",
                          (unsigned long)offset,unsigned(cutBytes));
            return false;
        }
#endif
        if(coded()) {bindInflater();return inflater.feed(offset,data,n);}
        return partition && offset%PAGE_BYTES==0 && n && n<=PAGE_BYTES &&
            offset<=partition->size && PAGE_BYTES<=partition->size-offset &&
            esp_partition_erase_range(partition,offset,PAGE_BYTES)==ESP_OK &&
            esp_partition_write(partition,offset,data,n)==ESP_OK;
    }
    bool same(uint32_t offset,const uint8_t* data,size_t n) override {
        // Flash holds decoded pages, not the container: a late duplicate of an
        // authenticated block is accepted without comparison.
        if(coded())return true;
        uint8_t back[WindowGeometry::MAX_CHUNK];return partition && n<=sizeof back &&
            esp_partition_read(partition,offset,back,n)==ESP_OK && !memcmp(data,back,n);
    }
    bool verifySample(const WindowManifest& manifest) {
        uint8_t sha[32];return hashPartition(partition,manifest.decodedSize(),sha) && !memcmp(sha,manifest.sha,32);
    }
    bool finish(const WindowManifest& manifest) override {
        if(manifest.coding==WindowManifest::DEFLATE_PAGES && !(current.same(manifest) && inflater.complete()))return false;
        uint8_t sha[32];if(!hashPartition(partition,manifest.decodedSize(),sha)||memcmp(sha,manifest.sha,32))return false;
        esp_partition_pos_t pos{partition->address,partition->size};esp_image_metadata_t metadata{};
        return esp_image_verify(ESP_IMAGE_VERIFY,&pos,&metadata)==ESP_OK;
    }
    bool activate(const WindowManifest& manifest) override {
        if(!partition || !finish(manifest))return false;
        Preferences p;if(!p.begin("rf-ota",false))return false;
        bool ok=p.putUInt("candidate",partition->address)==sizeof(uint32_t) &&
            p.putUInt("size",manifest.decodedSize())==sizeof(uint32_t) && p.putBytes("sha",manifest.sha,32)==32;
        p.end();return ok && esp_ota_set_boot_partition(partition)==ESP_OK;
    }
    bool discard() override {
        Preferences p;if(!p.begin("ota-pages",false))return false;
        bool ok=p.clear();p.end();
        current={};
#ifdef RFOTA_DIAGNOSTICS
        if(ok)cutArmed=cutPaused=false;
#endif
        return ok;
    }
};
}
