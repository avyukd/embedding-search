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

#include "constants.h"
#include "distance_metric.h"

namespace fs = std::filesystem;

class EmbeddingStore {
    private:
    uint32_t embedding_size;

    uint32_t max_object_store_size;
    uint32_t max_embedding_store_size;
    uint32_t max_embedding_to_object_map_size;

    int embedding_store_fd = -1;
    char* embedding_store_mmap_addr = nullptr;
    uint32_t embedding_store_write_idx = 0; 

    int embedding_to_object_map_fd = -1;
    char* embedding_to_object_map_mmap_addr = nullptr;
    uint32_t embedding_to_object_map_write_idx = 0;

    int object_store_fd = -1;
    char* object_store_mmap_addr = nullptr;
    uint32_t object_store_write_idx = 0;

    void mmap_path(const fs::path& path, int& fd, char*& mmap_addr, uint32_t max_size){
        const char* path_str = path.c_str();
        
        fd = open(path_str, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        assert(fd != -1);

        int alloc = ftruncate(embedding_store_fd, max_size);
        assert(alloc != -1);

        mmap_addr = (char*) mmap(
            NULL, max_embedding_store_size, PROT_READ | PROT_WRITE, MAP_SHARED, embedding_store_fd, 0);
        assert(mmap_addr != MAP_FAILED);
    }

    void memset_mmap_files(){
        memset(embedding_store_mmap_addr, 0, max_embedding_store_size);
        memset(embedding_to_object_map_mmap_addr, 0, max_embedding_to_object_map_size);
        memset(object_store_mmap_addr, 0, max_object_store_size);
    }

    bool set_write_indices(){
        while(
            embedding_store_mmap_addr[embedding_store_write_idx] != 0 && 
            embedding_store_write_idx < max_embedding_store_size
        ){
            embedding_store_write_idx++;
        }

        while(
            embedding_to_object_map_mmap_addr[embedding_to_object_map_write_idx] != 0 && 
            embedding_to_object_map_write_idx < max_embedding_to_object_map_size
        ){
            embedding_to_object_map_write_idx++;
        }

        while(
            object_store_mmap_addr[object_store_write_idx] != 0 && 
            object_store_write_idx < max_object_store_size
        ){
            object_store_write_idx++;
        }

        return embedding_store_write_idx < max_embedding_store_size && 
            embedding_to_object_map_write_idx < max_embedding_to_object_map_size &&
            object_store_write_idx < max_object_store_size;
    }

    void close_and_unmap_mmap_files(){
        close(embedding_store_fd);
        close(embedding_to_object_map_fd);
        close(object_store_fd);

        munmap(embedding_store_mmap_addr, max_embedding_store_size);
        munmap(embedding_to_object_map_mmap_addr, max_embedding_to_object_map_size);
        munmap(object_store_mmap_addr, max_object_store_size);
    }

    bool check_bounds(uint32_t value_size = 0){
        if(embedding_store_write_idx + embedding_size * sizeof(float) > max_embedding_store_size){
            return false;
        }
        if(embedding_to_object_map_write_idx + sizeof(uint32_t) > max_embedding_to_object_map_size){
            return false;
        }
        if(object_store_write_idx + value_size > max_object_store_size){
            return false;
        }
        return true;
    }

    public:

    EmbeddingStore(
        const char* dir, uint32_t embedding_size, 
        uint32_t max_embedding_store_size = DEFAULT_MAX_EMBEDDING_STORE_SIZE,
        uint32_t max_object_store_size = DEFAULT_MAX_OBJECT_STORE_SIZE
    ) : 
        max_object_store_size(max_object_store_size),
        max_embedding_store_size(max_embedding_store_size),
        embedding_size(embedding_size),
        max_embedding_to_object_map_size(max_embedding_store_size / embedding_size * sizeof(uint32_t))
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

        mmap_path(embedding_store_path, embedding_store_fd, embedding_store_mmap_addr, max_embedding_store_size);
        mmap_path(embedding_to_object_map_path, embedding_to_object_map_fd, embedding_to_object_map_mmap_addr, 
            max_embedding_to_object_map_size);
        mmap_path(object_store_path, object_store_fd, object_store_mmap_addr, max_object_store_size);

        if(num_paths_exist == 0){
            memset_mmap_files();
        }else{
            if(!set_write_indices()) throw std::runtime_error("Unable to set write indices. Mmap files are likely full.");
        }
    }

    ~EmbeddingStore(){
        close_and_unmap_mmap_files();
    }

    int add_embedding(float* embedding, std::string value){
        uint32_t value_num_chars = value.size() + 1;

        if(!check_bounds(value_num_chars)) return -1;

        uint32_t num_embedding_bytes = embedding_size * sizeof(float);
        memcpy(embedding_store_mmap_addr + embedding_store_write_idx, embedding, num_embedding_bytes);
        memcpy(embedding_to_object_map_mmap_addr + embedding_to_object_map_write_idx, &object_store_write_idx, OBJECT_STORE_IDX_TYPE_SIZE);
        
        memcpy(object_store_mmap_addr + object_store_write_idx, &value_num_chars, OBJECT_STORE_IDX_TYPE_SIZE);
        memcpy(object_store_mmap_addr + object_store_write_idx + OBJECT_STORE_IDX_TYPE_SIZE, value.c_str(), value_num_chars);

        embedding_store_write_idx += num_embedding_bytes;
        embedding_to_object_map_write_idx += OBJECT_STORE_IDX_TYPE_SIZE;
        object_store_write_idx += value_num_chars + OBJECT_STORE_IDX_TYPE_SIZE;

        return 0;
    }   

    std::vector<std::string> get_k_closest(
        float* embedding, uint32_t k, 
        uint32_t num_threads = 1, DistanceMetric metric = DistanceMetric::cosine_similarity){

        uint32_t num_rows = embedding_store_write_idx / (embedding_size * sizeof(float));
        uint32_t num_rows_per_thread = num_rows / num_threads;
        uint32_t leftover = num_rows % num_threads;
        
        std::mutex pq_mutex;
        std::priority_queue<std::pair<float, uint32_t>, 
            std::vector<std::pair<float, uint32_t>>,
            std::less<std::pair<float, uint32_t>>
        > pq; // max heap with distance, idx

        // each thread goes through its rows and adds to pq

        std::vector<std::thread> threads;
        for(size_t i = 0 ; i < num_threads; i++){
            uint32_t start = i * num_rows_per_thread;
            uint32_t end = start + num_rows_per_thread;
            if(i == num_threads - 1){
                end += leftover;
            }

            std::thread t([this, embedding, &pq, start, end, metric, k, &pq_mutex](){
                for(uint32_t i = start; i < end; i++){
                    float distance = compute_distance(
                        embedding,
                        embedding_store_mmap_addr + i * this->embedding_size * sizeof(float), 
                        embedding_size, 
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
        }

        for(auto& t : threads){
            t.join();
        }

        std::vector<std::string> result;
        while(!pq.empty()){
            auto [_, idx] = pq.top(); pq.pop();
            uint32_t object_store_offset = char_to_uint32_t(
                embedding_to_object_map_mmap_addr + idx * OBJECT_STORE_IDX_TYPE_SIZE,
                OBJECT_STORE_IDX_TYPE_SIZE
            );
            uint32_t value_num_chars = char_to_uint32_t(
                object_store_mmap_addr + object_store_offset,
                OBJECT_STORE_IDX_TYPE_SIZE
            );
            std::string value = std::string(
                object_store_mmap_addr + object_store_offset + OBJECT_STORE_IDX_TYPE_SIZE,
                value_num_chars
            );
            result.push_back(value);
        }

        std::reverse(result.begin(), result.end());
        return result;

    }
};

