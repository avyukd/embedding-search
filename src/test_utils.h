#pragma once
#include <filesystem>
#include <iostream>

#include "embedding_store.h"

#define IGNORE_TEST if(0)
#define ASSERT(x) if(!(x)){tw.~TestWrapper();assert(x);}
#define PRINT_VEC(x) for(auto i : x){std::cout << i << " ";}
#define PRINT_VEC_SECOND(x) for(auto i : x){std::cout << i.second << " ";}

namespace fs = std::filesystem;

class TestWrapper {
    private:
        fs::path dir_path;
    public:
        TestWrapper(const std::string& dir) : dir_path(dir) {
            if(!fs::exists(dir_path)){
                fs::create_directories(dir_path);
            }
            std::cout << "---" << std::endl;
        }
        ~TestWrapper(){
            auto n = fs::remove_all(dir_path);
            std::cout << "Removed " << n << " files at " << dir_path.string() << std::endl;
        }
        fs::path get_dir_path(){
            return dir_path;
        }
};

bool close(float a, float b){
    return std::abs(a - b) < 1e-6;
}

int add_embeddings(EmbeddingStore& store, std::vector<std::pair<std::vector<float>, std::string>> embeddings){
    int codes = 0;
    for(auto [k, v] : embeddings){
        codes += store.add_embedding(k, v);
    }
    return codes;
}