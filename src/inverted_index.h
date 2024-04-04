#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <utility>

#include "file_wrapper.h"
#include "constants.h"

namespace fs = std::filesystem;

const uint32_t DEFAULT_BLOCK_SIZE = 64;

class InvertedIndex {
    private:
        FileWrapper index;
        uint32_t block_size;
        uint32_t key_length;

        void get_padded_buf(const std::string& key, char* buf){
            memset(buf, 0, key_length);
            memcpy(buf, key.c_str(), std::min((uint32_t) key.size(), key_length));
        }

    public:

    // each block includes the key + values -- with defaults, we can store (64-16) / 4 = 12 values
    InvertedIndex(const char* dir, uint32_t max_size, uint32_t block_size = DEFAULT_BLOCK_SIZE, uint32_t key_length = 16) : 
        index(fs::path(dir) / INVERTED_INDEX_FN, max_size),
        block_size(block_size),
        key_length(key_length)
    {}

    uint32_t get_num_keys(){
        // std::cout << "index.write_idx: " << index.write_idx << std::endl;
        return (index.write_idx - DEFAULT_WRITE_IDX) / block_size;
    }

    int insert_block_in_position(uint32_t pos, const std::string& key, const std::vector<uint32_t>& values, bool new_key){
        uint32_t num_keys = get_num_keys();
        // std::cout << "num_keys: " << num_keys << std::endl;
        // std::cout << "pos: " << pos << std::endl;

        if(pos > num_keys){
            return -1; // max should be pos == num_keys
        }

        if(new_key){
            if(pos != num_keys){
                // std::cout << "memmove" << std::endl;
                memmove(
                    index.get_start_addr() + (pos + 1) * block_size,
                    index.get_start_addr() + pos * block_size,
                    (num_keys - pos) * block_size
                );
                // std::cout << std::string(index.get_start_addr() + (pos + 1) * block_size, key_length) << std::endl;
            }

            char key_str_buf[key_length];
            get_padded_buf(key, key_str_buf);

            uint32_t block_write_idx = pos * block_size; 
            index.write(key_str_buf, key_length, block_write_idx);
            block_write_idx += key_length;

            for(uint32_t v : values){
                // std::cout << "v: " << v << std::endl;
                // std::cout << "block_write_idx: " << block_write_idx << std::endl;
                index.write(&v, sizeof(uint32_t), block_write_idx);
                block_write_idx += sizeof(uint32_t);
            }

            uint32_t num_filler_bytes = (block_size - key_length - values.size() * sizeof(uint32_t)) / sizeof(uint32_t);
            for(size_t i = 0; i < num_filler_bytes; i++){
                index.write(&MAX_UINT32, 4, block_write_idx); // fill remaining block w/ max value
                block_write_idx += sizeof(uint32_t);
            }

            index.write_idx += block_size;
        }else{
            // std::cout << "inserting into existing block" << std::endl;
            // get num of available spaces in block
            char* value_start_pos = index.get_start_addr() + pos * block_size + key_length;
            uint32_t curr_value = char_to_uint32_t(value_start_pos);
            char* curr_value_pos = value_start_pos;
            while(curr_value != MAX_UINT32 && (curr_value_pos - value_start_pos) < (block_size - key_length)){
                curr_value_pos += 4;
                curr_value = char_to_uint32_t(curr_value_pos);
            }

            const uint32_t num_available_bytes = block_size - key_length - (curr_value_pos - value_start_pos);
            uint32_t num_available_spaces = num_available_bytes / sizeof(uint32_t);

            // std::cout << "num_available_spaces: " << num_available_spaces << std::endl;

            uint32_t block_write_idx = pos * block_size + key_length + (curr_value_pos - value_start_pos);
            for(size_t i = 0 ; i < std::min(num_available_spaces, (uint32_t) values.size()); i++){
                index.write(&values[i], sizeof(uint32_t), block_write_idx);
                block_write_idx += sizeof(uint32_t);
            }

            if(values.size() > num_available_spaces){
                std::vector<uint32_t> remaining_values(values.begin() + num_available_spaces, values.end());
                insert_block_in_position(pos + 1, key, remaining_values, true);
            }
        }
    }

    std::pair<uint32_t, bool> bsearch(const std::string& key){
        char key_search_buf[key_length];
        get_padded_buf(key, key_search_buf);

        uint32_t num_keys = get_num_keys();
        // std::cout << "num_keys: " << num_keys << std::endl;
        if(num_keys == 0)
            return std::make_pair(0, false);

        int l = 0;
        int r = num_keys - 1;
        while(l <= r){
            int m = l + (r - l) / 2;
            int cmp = memcmp(key_search_buf, index.get_start_addr() + m * block_size, key_length);
            // std::cout << "l: " << l << " r: " << r << " m: " << m << std::endl;
            // std::cout << "cmp: " << cmp << std::endl;
            // std::cout << "key: " << key << std::endl;
            // std::cout << std::string(index.get_start_addr() + m * block_size, key_length) << std::endl;
            if(cmp == 0){
                return std::make_pair(m, true);
            }else if(cmp < 0){
                r = m - 1;
            }else{
                l = m + 1;
            }
        }
        return std::make_pair(l, false);
    }

    int insert(const std::string& key, const std::vector<uint32_t>& values){
        if(key.size() > key_length){
            return -1;
        }
        auto [idx, found] = bsearch(key);
        // std::cout << "idx: " << idx << std::endl;
        insert_block_in_position(idx, key, values, !found);
        return 0;
    }   

    std::vector<uint32_t> get_values_from_block(uint32_t block_idx){
        // std::cout << "block_idx: " << block_idx << std::endl;
        std::vector<uint32_t> values;
        char* start_value_pos = index.get_start_addr() + block_idx * block_size + key_length;
        char* value_pos = start_value_pos;
        uint32_t curr_value = char_to_uint32_t(value_pos);
        while(curr_value != MAX_UINT32 && (value_pos - start_value_pos) < (block_size - key_length)){
            char print_buf[4];
            memcpy(print_buf, value_pos, 4);
            //std::cout << "value_pos: " << print_buf << std::endl;
            //std::cout << "curr_value: " << curr_value << std::endl;
            values.push_back(curr_value);
            value_pos += 4;
            curr_value = char_to_uint32_t(value_pos);
        }
        return values;
    }

    std::vector<uint32_t> search(const std::string& key){
        char key_search_buf[key_length];
        get_padded_buf(key, key_search_buf);

        std::vector<uint32_t> values;
        auto [idx, found] = bsearch(key);
        if(!found)
            return values; 

        // std::cout << "search idx " << idx << std::endl;
        // std::cout << "found " << found << std::endl;

        int back_idx = idx;
        while(back_idx >= 0 
            && memcmp(key_search_buf, index.get_start_addr() + back_idx * block_size, key_length) == 0){
            std::vector<uint32_t> block_values = get_values_from_block(back_idx);
            values.insert(values.end(), block_values.begin(), block_values.end());
            back_idx--;
        }

        uint32_t forward_idx = idx + 1;
        while(forward_idx < get_num_keys()
            && memcmp(key_search_buf, index.get_start_addr() + forward_idx * block_size, key_length) == 0){
            std::vector<uint32_t> block_values = get_values_from_block(forward_idx);
            values.insert(values.end(), block_values.begin(), block_values.end());
            forward_idx++;
        }

        return values;
    }


};

// internal structure:
// each key is 4 bytes + fixed block size for x # of values
// we can have multiple identical keys 
// binary search to find relevant key
// binary search for insertion as well
// need to have write idx