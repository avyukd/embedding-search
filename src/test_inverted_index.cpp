#include <vector>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <stdint.h>
#include <unordered_set>
#include <string>
#include <numeric>

#include "inverted_index.h"
#include "test_utils.h"

struct DocCount {
    uint32_t idx;
    uint8_t count;
}__attribute__((packed));

bool operator!=(const DocCount& dc1, const DocCount& dc2){
    return dc1.idx != dc2.idx || dc1.count != dc2.count;
}

template <typename ValueType>
bool equal_vecs(std::vector<ValueType> v1, std::vector<ValueType> v2){
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
        
        InvertedIndex<uint32_t> ii(tw.get_dir_path().c_str(), 1024, 64, 16);
        
        std::vector<uint32_t> v1 = {1, 2, 3, 4, 5};
        ii.insert("key1", v1);
        
        std::vector<uint32_t> v2 = {12, 13, 14, 15, 16};
        ii.insert("key2", v2);

        
        std::vector<uint32_t> key1_vals = ii.search("key1");
        ASSERT(equal_vecs(key1_vals, v1));

        std::vector<uint32_t> key2_vals = ii.search("key2");
        std::cout << "key2_vals: ";
        PRINT_VEC(key2_vals);
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

    {
        TestWrapper tw("test_files/sorted_ii_test");

        std::cout << "TEST -- SORTED..." << std::endl;

        InvertedIndex<uint32_t> ii(tw.get_dir_path().c_str(), 1024, 64, 16);

        ii.insert("b", {2});
        ii.insert("a", {1});
        ii.insert("c", {3});
        ii.insert("e", {5});
        ii.insert("d", {4});

        uint32_t start_val = 1; 
        for (const std::string& key : {"a", "b", "c", "d", "e"}){
            uint32_t found_val = ii.search(key)[0];
            ASSERT(start_val == found_val);
            start_val++;
        }

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/dup_key_ii_test");

        std::cout << "TEST -- DUPLICATE KEYS..." << std::endl;

        InvertedIndex<uint32_t> ii(tw.get_dir_path().c_str(), 1024, 32, 16); // max 4 vals

        std::vector<uint32_t> vals(10);
        std::iota(vals.begin(), vals.end(), 0);

        for(auto v: vals)
            ii.insert("key", {v});
        
        std::vector<uint32_t> key_vals = ii.search("key");
        std::sort(key_vals.begin(), key_vals.end());

        ASSERT(equal_vecs(key_vals, vals));

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/custom_type_test");

        std::cout << "TEST -- CUSTOM TYPE..." << std::endl;

        InvertedIndex<DocCount> ii(tw.get_dir_path().c_str(), 1024, 40, 10); // max 6 vals

        std::vector<DocCount> vals_1 = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};
        ii.insert("key1", vals_1);

        std::vector<DocCount> vals_2 = {{12, 1}, {13, 2}, {14, 3}, {15, 4}, {16, 5}};
        ii.insert("key2", vals_2);

        std::vector<DocCount> key1_vals = ii.search("key1");
        ASSERT(equal_vecs(key1_vals, vals_1));

        std::vector<DocCount> key2_vals = ii.search("key2");
        ASSERT(equal_vecs(key2_vals, vals_2));
    }

}