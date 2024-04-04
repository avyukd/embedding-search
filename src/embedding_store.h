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
#include <unordered_map>

#include "constants.h"
#include "distance_metric.h"
#include "hybrid_metric.h"
#include "file_wrapper.h"

#include "inverted_index.h"
#include "inverted_index_utils.h"

#include "utils.h"

namespace fs = std::filesystem;

class EmbeddingStore {
    private:
    uint32_t embedding_size;

    uint32_t max_object_store_size;
    uint32_t max_embedding_store_size;
    uint32_t max_embedding_to_object_map_size;

    InvertedIndex* inverted_index = nullptr; // stores keys to object store idxs 

    FileWrapper embedding_store;
    FileWrapper embedding_to_object_map;
    FileWrapper object_store;

    uint32_t num_embeddings;

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
        inverted_index(nullptr),
        num_embeddings(0)
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

        embedding_store = FileWrapper(embedding_store_path, max_embedding_store_size);
        embedding_to_object_map = FileWrapper(embedding_to_object_map_path, max_embedding_to_object_map_size);
        object_store = FileWrapper(object_store_path, max_object_store_size);

        if(hybrid_search_enabled){
            // todo: scrutinize this default a bit more
            inverted_index = new InvertedIndex(dir, (max_embedding_store_size / embedding_size) * DEFAULT_BLOCK_SIZE);
        }
    }

    static EmbeddingStore create(
        const char* dir, uint32_t embedding_size, 
        uint32_t max_embedding_store_size = DEFAULT_MAX_EMBEDDING_STORE_SIZE,
        uint32_t max_object_store_size = DEFAULT_MAX_OBJECT_STORE_SIZE
    ){
        return EmbeddingStore(dir, embedding_size, max_embedding_store_size, max_object_store_size);
    }

    int add_embedding(std::vector<float> embedding, std::string value){
        if(embedding.size() != embedding_size)
            return -1;
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

        if(inverted_index){
            std::vector<std::string> keys = get_keys_from_string(value);
            for(const std::string& key : keys){
                inverted_index->insert(key, {num_embeddings}); // num_embeddings serves as a unique id for now, assuming no deletes
            }
        }

        num_embeddings++;
        return 0;    
    }   

    // hybrid search
    std::vector<std::pair<float, std::string>> get_k_closest_hybrid_weighted(
        std::string search_str, std::vector<float> embedding, uint32_t k, 
        uint32_t num_threads = 1, DistanceMetric distance_metric = DistanceMetric::cosine_similarity,
        float keyword_weight = 0.5
    ){
        if(!inverted_index){
            return {};
        }

        if(embedding.size() != embedding_size){
            return {};
        }

        if(keyword_weight == 0)
            return get_k_closest(embedding, k, num_threads, distance_metric);

        std::unordered_map<uint32_t, uint32_t> idx_to_count;
        std::vector<std::string> keys = get_keys_from_string(search_str);
        for(const std::string& key : keys){
            std::vector<uint32_t> vals = inverted_index->search(key);
            for(auto val: vals)
                idx_to_count[val]++;
        }

        auto comp = [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b){
                return a.second < b.second;
            };
        uint32_t min_count = std::min_element(idx_to_count.begin(), idx_to_count.end(), comp)->second;
        uint32_t max_count = std::max_element(idx_to_count.begin(), idx_to_count.end(), comp)->second;

        std::unordered_map<uint32_t, float> idx_to_score;
        for(auto [idx, count]: idx_to_count){
            float weight = (count - min_count) / (max_count - min_count); // normalize to [0, 1]
            idx_to_score[idx] = weight; // max score is 1, closest to 0 is best
        }

        return get_k_closest_helper(embedding, k, num_threads, distance_metric, 
            [&idx_to_score, &keyword_weight](uint32_t idx, float distance){
                return (1.0f - idx_to_score[idx]) * keyword_weight + distance * (1 - keyword_weight);
            }
        );

    }

    std::vector<std::pair<float, std::string>> get_k_closest(
        std::vector<float> embedding, uint32_t k, 
        uint32_t num_threads = 1, DistanceMetric metric = DistanceMetric::cosine_similarity
    ){
        return get_k_closest_helper(embedding, k, num_threads, metric, [](uint32_t idx, float distance){
            return distance;
        });
    }

    std::vector<std::pair<float, std::string>> get_k_closest_helper(
        std::vector<float> embedding, uint32_t k, 
        uint32_t num_threads, DistanceMetric metric,
        std::function<float(uint32_t, float)> custom_score_function
    ){

        if(embedding.size() != embedding_size){
            return {};
        }

        uint32_t num_rows = (embedding_store.write_idx - DEFAULT_WRITE_IDX) / (embedding_size * sizeof(float));
        uint32_t num_rows_per_thread = num_rows / num_threads;
        uint32_t leftover = num_rows % num_threads;
        assert(num_rows == num_embeddings);

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

            std::thread t([this, &embedding, &pq, start, end, metric, k, &pq_mutex, custom_score_function](){
                for(uint32_t i = start; i <= end; i++){
                    float distance = compute_distance(
                        embedding.data(),
                        this->embedding_store.mmap_addr + DEFAULT_WRITE_IDX + i * this->embedding_size * sizeof(float), 
                        this->embedding_size, 
                        metric
                    );

                    float score = custom_score_function(i, distance);

                    std::lock_guard<std::mutex> lock(pq_mutex);
                    if(pq.size() != k || score < pq.top().first){
                        pq.push(std::make_pair(score, i));
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
        delete inverted_index; // will call close(fd) -- should be ok?
    }
};

