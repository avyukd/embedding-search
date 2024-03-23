#include <vector>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <stdint.h>
#include <unordered_set>

#include "inverted_index.h"
#include "test_utils.h"

bool equal_vecs(std::vector<uint32_t> v1, std::vector<uint32_t> v2){
    if(v1.size() != v2.size()){
        return false;
    }
    for(int i = 0; i < v1.size(); i++){
        if(v1[i] != v2[i]){
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv){
    {
        TestWrapper tw("test_files/basic_ii_test");

        std::cout << "TEST -- BASIC..." << std::endl;
        
        InvertedIndex ii(tw.get_dir_path().c_str(), 1024, 64, 16);
        
        std::vector<uint32_t> v1 = {1, 2, 3, 4, 5};
        ii.insert("key1", v1);
        
        std::vector<uint32_t> v2 = {12, 13, 14, 15, 16};
        ii.insert("key2", v2);

        
        std::vector<uint32_t> key1_vals = ii.search("key1");
        ASSERT(equal_vecs(key1_vals, v1));

        std::vector<uint32_t> key2_vals = ii.search("key2");
        ASSERT(equal_vecs(key2_vals, v2));

        std::vector<uint32_t> v1_add = {6, 7, 8, 9, 10};
        ii.insert("key1", v1_add);

        std::vector<uint32_t> all_exp_key1_keys;
        all_exp_key1_keys.insert(all_exp_key1_keys.end(), v1.begin(), v1.end());
        all_exp_key1_keys.insert(all_exp_key1_keys.end(), v1_add.begin(), v1_add.end());
        
        std::vector<uint32_t> new_key1_vals = ii.search("key1");
        ASSERT(equal_vecs(new_key1_vals, all_exp_key1_keys));

        std::cout << "PASSED" << std::endl;
    }

}