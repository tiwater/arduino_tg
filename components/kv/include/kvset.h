/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#ifndef AOS_KVSET_H
#define AOS_KVSET_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// #define ENABLE_CACHE    1
#define INIT_CACHE_NUM  1

typedef struct kvset kv_t;
typedef struct flash_ops flash_ops_t;
typedef struct kvblock kvblock_t;
typedef struct kvnode kvnode_t;
#include <block.h>

struct flash_ops {
    int (*erase)(kv_t *kv, int pos, int size);
    int (*write)(kv_t *kv, int pos, void *data, int size);
    int (*read)(kv_t *kv, int pos, void *data, int size);
};

#ifdef ENABLE_CACHE
typedef struct cache_node {
    uint32_t hash;
    uint16_t block_id;
    uint16_t offset;
} cache_node_t;
#endif

struct kvnode {
    uint8_t    rw;
    uint8_t    version;
    uint16_t   val_size; /* value size */
    uint32_t   erase_flag;

    uint16_t  head_offset; /* offset base block */
    uint16_t  value_offset;
    uint16_t  next_offset;
    uint16_t  node_size;

    kvblock_t *block;
};

struct kvset {
    kvblock_t   *blocks;
    int          num;
    int          bid; // current block_id
    int          gc_bid;
    int          handle;
    uint8_t     *mem;
    flash_ops_t *ops;
#ifdef ENABLE_CACHE
    cache_node_t *cache;
    int          cache_num;
#endif
};

/**
 * @brief  init the kv fs 
 * @param  [in] kv
 * @param  [in] mem        : the start addrress of flash or mem, etc
 * @param  [in] block_num  : number of blocks
 * @param  [in] block_size : size of per-block
 * @return 0/-1 
 */
int kv_init(kv_t *kv, uint8_t *mem, int block_num, int block_size);

/**
 * @brief  reset the kv fs 
 * @param  [in] kv
 * @return 0/-1 
 */
int kv_reset(kv_t *kv);

/**
 * @brief  set key-value pair 
 * @param  [in] kv
 * @param  [in] key
 * @param  [in] value
 * @param  [in] size  : size of the value
 * @return size on success 
 */
int kv_set(kv_t *kv, const char *key, void *value, int size);

/**
 * @brief  get value by the key-string
 * @param  [in] kv
 * @param  [in] key
 * @param  [in] value
 * @param  [in] size  : size of the value
 * @return > 0 on success 
 */
int kv_get(kv_t *kv, const char *key, void *value, int size);

/**
 * @brief  delete the key from kv fs 
 * @param  [in] kv
 * @param  [in] key
 * @return 0 on success 
 */
int kv_rm(kv_t *kv, const char *key);

/**
 * @brief  find the kvnode by key  
 * @param  [in] kv
 * @param  [in] key
 * @param  [in] node : used for store the result finding
 * @return 0 if find 
 */
int kv_find(kv_t *kv, const char *key, kvnode_t *node);

/**
 * @brief  iterate all valid kv pair  
 * @param  [in] kv
 * @param  [in] fn   : callback
 * @param  [in] data : opaque of the fn callback
 * @return 0 on success 
 */
int kv_iter(kv_t *kv, int (*fn)(kvnode_t *, void *), void *data);

/**
 * @brief  kv garbage collect 
 * @param  [in] kv
 * @return 0/-1 
 */
int  kv_gc(kv_t *kv);

/**
 * @brief  dump all the kv pair to stdout
 * @param  [in] kv
 * @return  
 */
void kv_dump(kv_t *kv);

/**
 * @brief  show all kv to stdout in hex
 * @param  [in] kv
 * @return  
 */
void kv_show_data(kv_t *kv);

void kv_cache_in(kv_t *kv, const char *key, int block_id, uint32_t offset);
void kv_cache_out(kv_t *kv, int block_id, uint32_t offset);

#define KVNODE_OFFSET2CACHE(kv_node, kv_offset) ((kv_node)->block->mem_cache + (kv_node)->kv_offset)
#define KVNODE_CACHE2OFFSET(kv_node, mem_addr) (mem_addr - (kv_node)->block->mem_cache)

#ifdef __cplusplus
}
#endif

#endif
