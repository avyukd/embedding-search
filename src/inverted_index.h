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

class InvertedIndex {
    private:
        FileWrapper index;
        uint32_t block_size;
        uint32_t key_length;

    public:

    // each block includes the key + values -- with defaults, we can store (64-16) / 4 = 12 values
    InvertedIndex(const char* dir, uint32_t max_size, uint32_t block_size = 64, uint32_t key_length = 16) : 
        index(fs::path(dir) / INVERTED_INDEX_FN, max_size),
        block_size(block_size),
        key_length(key_length)
    {}

    uint32_t get_num_keys(){
        return (index.write_idx - DEFAULT_WRITE_IDX) / block_size;
    }

    int insert_block_in_position(uint32_t pos, const std::string& key, const std::vector<uint32_t>& values, bool new_key){
        uint32_t num_keys = get_num_keys();

        if(pos > num_keys){
            return -1; // max should be pos == num_keys
        }

        if(new_key){
            if(pos != num_keys){
                memmove(
                    index.get_start_addr() + (pos + 1) * block_size,
                    index.get_start_addr() + pos * block_size,
                    (num_keys - pos) * block_size
                );
            }
            
            uint32_t block_write_idx = pos * block_size; 
            index.write(key.c_str(), key_length, block_write_idx);
            block_write_idx += key_length;

            for(uint32_t v : values){
                index.write(&v, sizeof(uint32_t), block_write_idx);
                block_write_idx += sizeof(uint32_t);
            }

            for(size_t i = 0; i < (block_size - key_length - values.size() ); i++){
                index.write(&MAX_UINT32, 4, block_write_idx); // fill remaining block w/ max value
                block_write_idx += sizeof(uint32_t);
            }

            index.write_idx += block_size;
        }else{
            // get num of available spaces in block
            char* value_start_pos = index.get_start_addr() + pos * block_size + key_length;
            uint32_t curr_value = char_to_uint32_t(value_start_pos);
            char* curr_value_pos = value_start_pos;
            while(curr_value != MAX_UINT32){
                curr_value_pos += 4;
                curr_value = char_to_uint32_t(curr_value_pos);
            }

            const uint32_t num_available_bytes = block_size - key_length - (curr_value_pos - value_start_pos);
            uint32_t num_available_spaces = num_available_bytes / sizeof(uint32_t);

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
        uint32_t num_keys = get_num_keys();
        std::cout << "num_keys: " << num_keys << std::endl;
        if(num_keys == 0)
            return std::make_pair(0, false);

        uint32_t l = 0;
        uint32_t r = num_keys - 1;
        while(l <= r){
            uint32_t m = l + (r - l) / 2;
            int cmp = memcmp(key.c_str(), index.get_start_addr() + m * block_size, key_length);
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
        insert_block_in_position(idx, key, values, !found);
        return 0;
    }   

    std::vector<uint32_t> get_values_from_block(uint32_t block_idx){
        std::vector<uint32_t> values;
        char* value_pos = index.get_start_addr() + block_idx * block_size + key_length;
        uint32_t curr_value = char_to_uint32_t(value_pos);
        while(curr_value != MAX_UINT32){
            values.push_back(curr_value);
            value_pos += 4;
            curr_value = char_to_uint32_t(value_pos);
        }
        return values;
    }

    std::vector<uint32_t> search(const std::string& key){
        std::vector<uint32_t> values;
        auto [idx, found] = bsearch(key);
        if(!found)
            return values; 

        uint32_t back_idx = idx;
        while(back_idx >= 0 
            && memcmp(key.c_str(), index.get_start_addr() + back_idx * block_size, key_length) == 0){
            std::vector<uint32_t> block_values = get_values_from_block(back_idx);
            values.insert(values.end(), block_values.begin(), block_values.end());
            back_idx--;
        }

        uint32_t forward_idx = idx + 1;
        while(forward_idx < get_num_keys()
            && memcmp(key.c_str(), index.get_start_addr() + forward_idx * block_size, key_length) == 0){
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