#pragma once
#include <Preferences.h>
#include <esp_image_format.h>
#include "flash_store.h"
#include "window_session.h"

namespace labota {
class WindowFlashStore : public WindowStore {
    const esp_partition_t* partition=nullptr;
    static constexpr uint32_t MAGIC=0x574f5431;
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
        uint8_t record[52]{};
        bool found=p.getBytes("checkpoint",record,sizeof record)==sizeof record;p.end();
        partition=esp_ota_get_next_update_partition(nullptr);
        if(!found || !partition || get32(record)!=MAGIC || get32(record+4)!=partition->address)return false;
        manifest.size=get32(record+8);committed=get32(record+12);memcpy(manifest.sha,record+16,32);
        return get32(record+48)==BOARD && manifest.size && manifest.size<=partition->size;
    }
    bool checkpoint(const WindowManifest& manifest,uint32_t committed) override {
        partition=esp_ota_get_next_update_partition(nullptr);
        if(!partition || manifest.size>partition->size)return false;
        uint8_t record[52]{};put32(record,MAGIC);put32(record+4,partition->address);
        put32(record+8,manifest.size);put32(record+12,committed);memcpy(record+16,manifest.sha,32);put32(record+48,BOARD);
        Preferences p;if(!p.begin("ota-pages",false))return false;
        bool ok=p.putBytes("checkpoint",record,sizeof record)==sizeof record;p.end();return ok;
    }
    bool page(uint32_t offset,const uint8_t* data,size_t n) override {
#ifdef RFOTA_DIAGNOSTICS
        if(cutPaused)return false;
        if(cutArmed && offset==cutOffset) {
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
        return partition && offset%PAGE_BYTES==0 && n && n<=PAGE_BYTES &&
            offset<=partition->size && PAGE_BYTES<=partition->size-offset &&
            esp_partition_erase_range(partition,offset,PAGE_BYTES)==ESP_OK &&
            esp_partition_write(partition,offset,data,n)==ESP_OK;
    }
    bool same(uint32_t offset,const uint8_t* data,size_t n) override {
        uint8_t back[WINDOW_CHUNK];return partition && n<=sizeof back &&
            esp_partition_read(partition,offset,back,n)==ESP_OK && !memcmp(data,back,n);
    }
    bool finish(const WindowManifest& manifest) override {
        uint8_t sha[32];if(!hashPartition(partition,manifest.size,sha)||memcmp(sha,manifest.sha,32))return false;
        esp_partition_pos_t pos{partition->address,partition->size};esp_image_metadata_t metadata{};
        return esp_image_verify(ESP_IMAGE_VERIFY,&pos,&metadata)==ESP_OK;
    }
    bool activate(const WindowManifest& manifest) override {
        if(!partition || !finish(manifest))return false;
        Preferences p;if(!p.begin("rf-ota",false))return false;
        bool ok=p.putUInt("candidate",partition->address)==sizeof(uint32_t) &&
            p.putUInt("size",manifest.size)==sizeof(uint32_t) && p.putBytes("sha",manifest.sha,32)==32;
        p.end();return ok && esp_ota_set_boot_partition(partition)==ESP_OK;
    }
    bool discard() override {
        Preferences p;if(!p.begin("ota-pages",false))return false;
        bool ok=p.clear();p.end();
#ifdef RFOTA_DIAGNOSTICS
        if(ok)cutArmed=cutPaused=false;
#endif
        return ok;
    }
};
}
