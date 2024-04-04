#pragma once
#include <stdint.h>

using OBJECT_STORE_IDX_TYPE = uint32_t;
const uint32_t OBJECT_STORE_IDX_TYPE_SIZE = sizeof(OBJECT_STORE_IDX_TYPE);

const uint32_t DEFAULT_WRITE_IDX = sizeof(uint32_t);

const char* EMBEDDING_STORE_FN = "embedding_store.bin";
const char* EMBEDDING_TO_OBJECT_MAP_FN = "embedding_to_object_map.bin";
const char* OBJECT_STORE_FN = "object_store.bin";
const char* INVERTED_INDEX_FN = "inverted_index.bin";

uint32_t DEFAULT_MAX_EMBEDDING_STORE_SIZE = 1024 * 1024; // 1024 vectors of 1024 bytes each
uint32_t DEFAULT_MAX_OBJECT_STORE_SIZE = 1024 * 1024; // 1024 objects of 1024 bytes each

const uint32_t MAX_UINT32 = 0xffffffff;

const char* COMMON_WORDS_FN = "common_words.txt";