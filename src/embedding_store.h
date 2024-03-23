#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <queue>

#include <mutex>
#include <thread>
#include <functional>
#include <filesystem>
#include <iostream>

#include "constants.h"
#include "distance_metric.h"
#include "file_wrapper.h"
//#include "inverted_index.h"
#include "utils.h"

namespace fs = std::filesystem;

class EmbeddingStore {
    private:
    uint32_t embedding_size;

    uint32_t max_object_store_size;
    uint32_t max_embedding_store_size;
    uint32_t max_embedding_to_object_map_size;

    bool hybrid_search_enabled;

    FileWrapper embedding_store;
    FileWrapper embedding_to_object_map;
    FileWrapper object_store;

    public:

    EmbeddingStore(
        const char* dir, uint32_t embedding_size, 
        uint32_t max_embedding_store_size = DEFAULT_MAX_EMBEDDING_STORE_SIZE,
        uint32_t max_object_store_size = DEFAULT_MAX_OBJECT_STORE_SIZE,
        bool hybrid_search_enabled = false
    ) : 
        embedding_size(embedding_size),
        max_object_store_size(max_object_store_size),
        max_embedding_store_size(max_embedding_store_size),
        max_embedding_to_object_map_size(max_embedding_store_size / embedding_size * sizeof(uint32_t)),
        hybrid_search_enabled(hybrid_search_enabled)
    {
        fs::path dir_path = dir;
        if(!fs::exists(dir_path)){
            fs::create_directories(dir_path);
        }

        fs::path embedding_store_path = dir_path / EMBEDDING_STORE_FN;
        fs::path embedding_to_object_map_path = dir_path / EMBEDDING_TO_OBJECT_MAP_FN;
        fs::path object_store_path = dir_path / OBJECT_STORE_FN;

        int num_paths_exist = get_num_paths_exist({embedding_store_path, embedding_to_object_map_path, object_store_path});
        if(num_paths_exist != 0 && num_paths_exist != 3){
            throw std::runtime_error("All or none of the files should exist, but this is not the case. Invalid state.");
        }

        embedding_store.init(embedding_store_path, max_embedding_store_size);
        embedding_to_object_map.init(embedding_to_object_map_path, max_embedding_to_object_map_size);
        object_store.init(object_store_path, max_object_store_size);
    }

    static EmbeddingStore create(
        const char* dir, uint32_t embedding_size, 
        uint32_t max_embedding_store_size = DEFAULT_MAX_EMBEDDING_STORE_SIZE,
        uint32_t max_object_store_size = DEFAULT_MAX_OBJECT_STORE_SIZE
    ){
        return EmbeddingStore(dir, embedding_size, max_embedding_store_size, max_object_store_size);
    }

    int add_embedding(std::vector<float> embedding, std::string value){
        if(embedding.size() != embedding_size){
            return -1;
        }
        // todo: everything is messed up if you write past eof on one file -- no good recovery rn
        uint32_t value_size = value.size();
        if(embedding_store.write(embedding.data(), embedding_size * sizeof(float)) < 0)
            return -2;
        if(embedding_to_object_map.write(&object_store.write_idx, OBJECT_STORE_IDX_TYPE_SIZE) < 0)
            return -3;
        if(object_store.write(&value_size, OBJECT_STORE_IDX_TYPE_SIZE) < 0)
            return -4;
        if(object_store.write(value.c_str(), value_size) < 0) // won't copy null terminating char
            return -5;
        return 0;    
    }   

    std::vector<std::pair<float, std::string>> get_k_closest(
        std::vector<float> embedding, uint32_t k, 
        uint32_t num_threads = 1, DistanceMetric metric = DistanceMetric::cosine_similarity){

        if(embedding.size() != embedding_size){
            return {};
        }

        uint32_t num_rows = (embedding_store.write_idx - DEFAULT_WRITE_IDX) / (embedding_size * sizeof(float));
        uint32_t num_rows_per_thread = num_rows / num_threads;
        uint32_t leftover = num_rows % num_threads;

        // std::cout << "num_rows: " << num_rows << std::endl;
        // std::cout << "num_rows_per_thread: " << num_rows_per_thread << std::endl;
        // std::cout << "leftover: " << leftover << std::endl;

        std::mutex pq_mutex;
        std::priority_queue<std::pair<float, uint32_t>, 
            std::vector<std::pair<float, uint32_t>>,
            std::less<std::pair<float, uint32_t>>
        > pq; // max heap with distance, idx

        // each thread goes through its rows and adds to pq

        std::vector<std::thread> threads;
        uint32_t start = 0;
        for(size_t i = 0 ; i < num_threads; i++){
            uint32_t end = start + num_rows_per_thread;
            if(i == num_threads - 1){
                end += leftover;
            }
            // std::cout << "start: " << start << " end: " << end << std::endl;

            std::thread t([this, &embedding, &pq, start, end, metric, k, &pq_mutex](){
                for(uint32_t i = start; i <= end; i++){
                    float distance = compute_distance(
                        embedding.data(),
                        this->embedding_store.mmap_addr + DEFAULT_WRITE_IDX + i * this->embedding_size * sizeof(float), 
                        this->embedding_size, 
                        metric
                    );

                    std::lock_guard<std::mutex> lock(pq_mutex);
                    if(pq.size() != k || distance < pq.top().first){
                        pq.push(std::make_pair(distance, i));
                        if(pq.size() > k){
                            pq.pop();
                        }
                    }
                }
            });
            threads.push_back(std::move(t));

            start = end + 1;
        }

        for(auto& t : threads){
            t.join();
        }

        std::vector<std::pair<float, std::string>> result;
        while(!pq.empty()){
            auto [dist, idx] = pq.top(); pq.pop();
            uint32_t object_store_offset = char_to_uint32_t(embedding_to_object_map.mmap_addr + DEFAULT_WRITE_IDX + idx * OBJECT_STORE_IDX_TYPE_SIZE);
            // object store offset is an index which includes DEFAULT_WRITE_IDX
            uint32_t value_size = char_to_uint32_t(object_store.mmap_addr + object_store_offset); 
            std::string value = std::string(
                object_store.mmap_addr + object_store_offset + OBJECT_STORE_IDX_TYPE_SIZE,
                value_size
            );
            result.push_back(std::make_pair(dist, value));
        }

        reverse(result.begin(), result.end());
        return result;
    }

    void close_store(){ // for the python binding
        embedding_store.set_write_idx();
        embedding_to_object_map.set_write_idx();
        object_store.set_write_idx();
    }
};

