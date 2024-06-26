#include <vector>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <stdint.h>
#include <unordered_set>

#include "embedding_store.h"
#include "test_utils.h"

int main(int argc, char** argv){
    {
        TestWrapper tw("test_files/basic_test");

        std::cout << "TEST -- BASIC..." << std::endl;
        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024);        

        std::vector<float> v1 = {1.0, 1.0};
        std::vector<float> v2 = {0.0, 1.0};
        std::vector<float> v3 = {1.0, 0.0};
        std::vector<float> v4 = {0.0, 0.0};
        std::vector<float> v5 = {0.9, 0.9};

        int code = add_embeddings(store, {
            {v1, "v1"}, 
            {v2, "v2"}, 
            {v3, "v3"}, 
            {v4, "v4"}, 
            {v5, "v5"}
        });
        ASSERT(code == 0);

        auto closest = store.get_k_closest(v1, 2, 1, DistanceMetric::cosine_similarity);
        ASSERT(closest.size() == 2);
        ASSERT(closest[0].second == "v1");
        ASSERT(closest[1].second == "v5");

        std::vector<float> v6 = {1.2, 1.0};
        store.add_embedding(v6, "v6");

        std::vector<float> v7 = {1.2, 1};
        closest = store.get_k_closest(v7, 1, 1, DistanceMetric::cosine_similarity);
        ASSERT(closest[0].second == "v6");

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/load_test");
        std::cout << "TEST -- LOAD..." << std::endl;
        std::vector<float> k2 = {0.2, 0.4}; std::string v2 = "basketball";
        {
            std::cout << "Add embeddings..." << std::endl;
            EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024);        
        
            std::vector<float> k1 = {0.2, 0.3}; std::string v1 = "horse";
            std::vector<float> k3 = {1.2, 2.5}; std::string v3 = "football";

            add_embeddings(store, {
                {k1, v1}, {k2, v2}, {k3, v3}
            });
        }
        {
            std::cout << "Load embeddings..." << std::endl;
            EmbeddingStore store(tw.get_dir_path().c_str(), 2);        

            std::vector<float> k1 = {0.2, 0.3};
            auto closest = store.get_k_closest(k1, 3, 1, DistanceMetric::cosine_similarity);
            ASSERT(closest.size() == 3);
            ASSERT(closest[0].second == "horse");
            ASSERT(closest[1].second == "basketball");
            ASSERT(closest[2].second == "football");

            std::vector<float> k4 = {0.7, 0.7}; std::string v4 = "nascar";
            store.add_embedding(k4, v4);

            auto closest_second = store.get_k_closest(k2, 1, 1, DistanceMetric::cosine_similarity);
            ASSERT(closest_second.size() == 1);
            ASSERT(closest_second[0].second == "basketball");
        }
        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/multi_thread_test");
        std::cout << "TEST -- MULTI THREAD..." << std::endl;

        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024);
        std::vector<float> keys = {1, 2, 3, 4, 5};
        for(float k1 : keys){
            for(float k2 : keys){
                std::string v = std::to_string((int) k1) + std::to_string((int) k2);
                store.add_embedding({k1, k2}, v);
            }
        }

        auto closest = store.get_k_closest({1, 1}, 5, 2, DistanceMetric::cosine_similarity);
        ASSERT(closest.size() == 5);
        for(auto c : closest){
            ASSERT(close(c.first, 0.));
        }
        ASSERT(closest[0].second == "11");
        ASSERT(closest[1].second == "22");
        ASSERT(closest[2].second == "33");
        ASSERT(closest[3].second == "44");
        ASSERT(closest[4].second == "55");

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/multi_thread_test");
        std::cout << "TEST -- MULTI THREAD EDGE 1..." << std::endl;

        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024);
        std::vector<float> keys = {1, 2, 3, 4, 5};

        size_t i = 0; 
        for(float k1 : keys){
            if(i >= 10){
                break;
            }
            for(float k2 : keys){
                std::string v = std::to_string((int) k1) + std::to_string((int) k2);
                store.add_embedding({k1, k2}, v);
                i++;
            }
        }

        auto closest = store.get_k_closest({1, 1}, 10, 10, DistanceMetric::cosine_similarity);
        ASSERT(closest.size() == 10);

        auto closest_values = std::unordered_set<std::string>();
        for(auto c : closest){
            closest_values.insert(c.second);
        }
        ASSERT(closest_values.size() == 10);

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/multi_dim_test");
        std::cout << "TEST -- MULTI DIM..." << std::endl;

        EmbeddingStore store(tw.get_dir_path().c_str(), 5, 1024, 1024);

        std::vector<float> keys = {1, 2, 3, 4, 5};
        for(float k1 : keys){
            for(float k2 : keys){
                store.add_embedding({k1, k2, k1, k2, k1}, "other");
            }
        }

        for(float f = 1 ; f < 6. ; f += 1.){
            store.add_embedding({f * 2, f, f, f, f}, "identical");
        }

        auto closest = store.get_k_closest({2, 1, 1, 1, 1}, 5, 1, DistanceMetric::cosine_similarity);
        ASSERT(closest.size() == 5);
        for(auto c : closest){
            ASSERT(close(c.first, 0.));
            ASSERT(c.second == "identical");
        }
        std::cout << "PASSED" << std::endl;
    }


}