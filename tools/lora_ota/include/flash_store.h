#pragma once
#include <algorithm>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include "session.h"
namespace labota {
inline bool hashPartition(const esp_partition_t* part,uint32_t bytes,uint8_t digest[32]) {
    if(!part || bytes>part->size) return false;
    mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);
    bool ok=mbedtls_sha256_starts_ret(&ctx,0)==0;
    uint8_t block[512];
    for(uint32_t pos=0;pos<bytes && ok;) {
        size_t n=std::min(size_t(bytes-pos),sizeof block);
        ok=esp_partition_read(part,pos,block,n)==ESP_OK;
        if(ok) ok=mbedtls_sha256_update_ret(&ctx,block,n)==0;
        pos+=n;
    }
    ok=ok && mbedtls_sha256_finish_ret(&ctx,digest)==0;
    mbedtls_sha256_free(&ctx);return ok;
}
class FlashStore : public Store {
public:
    const esp_partition_t* partition=nullptr;
    esp_ota_handle_t handle=0;
    uint32_t bytes=0;
    uint32_t capacity() const override {auto p=esp_ota_get_next_update_partition(nullptr);return p?p->size:0;}
    bool begin(uint32_t size) override {
        partition=esp_ota_get_next_update_partition(nullptr);bytes=size;
        return partition && esp_ota_begin(partition,size,&handle)==ESP_OK;
    }
    bool write(const uint8_t* p,size_t n) override {return esp_ota_write(handle,p,n)==ESP_OK;}
    bool same(uint32_t offset,const uint8_t* p,size_t n) override {
        uint8_t back[CHUNK];
        return n<=sizeof back && esp_partition_read(partition,offset,back,n)==ESP_OK && !memcmp(back,p,n);
    }
    bool finish(const uint8_t expected[32]) override {
        uint8_t digest[32];
        if(!hashPartition(partition,bytes,digest)||memcmp(expected,digest,32)) return false;
        auto result=esp_ota_end(handle);handle=0;return result==ESP_OK;
    }
    bool activate() override {return partition && esp_ota_set_boot_partition(partition)==ESP_OK;}
    void abort() override {if(handle)esp_ota_abort(handle);handle=0;partition=nullptr;}
};
}
