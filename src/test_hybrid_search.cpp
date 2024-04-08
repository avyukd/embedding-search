#include <vector>
#include <iostream>

#include "embedding_store.h"
#include "test_utils.h"

int main(int argc, char** argv){
    {
        TestWrapper tw("test_files/basic_hybrid_search_test");

        std::cout << "TEST -- BASIC..." << std::endl;
        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024, true);

        std::vector<float> v1 = {1.0, 1.0};
        std::vector<float> v2 = {0.0, 1.0};
        std::vector<float> v3 = {1.0, 0.0};
        std::vector<float> v4 = {0.9, 0.9};

        int code = add_embeddings(store, {
            {v1, "v1 v2 v3 v4"}, 
            {v2, "v1 v2"}, 
            {v3, "v3"}, 
            {v4, "v4"}
        });
        ASSERT(code == 0);

        auto closest = store.get_k_closest_hybrid_weighted(
            "v3", v1, 2, 1, DistanceMetric::cosine_similarity, 1
        );
        ASSERT(closest.size() == 2);
        // PRINT_VEC_SECOND(closest);
        ASSERT(closest[0].second == "v1 v2 v3 v4");
        ASSERT(closest[1].second == "v3");

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/string_preprocess_hybrid_search_test");
        
        std::cout << "TEST -- STRING PREPROCESS..." << std::endl;
        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024, true);

        std::string line_1 = "harry potter's favorite dessert was Butterbeer."; 
        std::string line_2 = "butterbeer!! I love it frr.";
        std::string line_3 = "SHUT UP";
        std::string line_4 = "potter-- shut your mouth!";


        int code = add_embeddings(store, {
            {{1.0, 1.0}, line_1}, 
            {{0.0, 1.0}, line_2}, 
            {{1.0, 0.0}, line_3}, 
            {{0.9, 0.9}, line_4}
        });
        ASSERT(code == 0);

        auto closest = store.get_k_closest_hybrid_weighted(
            "harry potter", {1.0, 1.0}, 2, 1, DistanceMetric::cosine_similarity, 1
        );
        ASSERT(closest.size() == 2);
        ASSERT(closest[0].second == line_1);
        ASSERT(closest[1].second == line_4);

        auto closest_2 = store.get_k_closest_hybrid_weighted(
            "butterbeer", {0.0, 1.0}, 2, 1, DistanceMetric::cosine_similarity, 1
        );
        ASSERT(closest_2.size() == 2);
        ASSERT(closest_2[0].second == line_1 || closest_2[0].second == line_2);
        ASSERT(closest_2[1].second == line_1 || closest_2[1].second == line_2); 

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/zero_weight_hybrid_search_test");

        std::cout << "TEST -- ZERO WEIGHT" << std::endl;
        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024, true);

        int code = add_embeddings(store, {
            {{1.0, 1.0}, "1"},
            {{0.0, 1.0}, "2"},
            {{1.0, 0.0}, "3"},
            {{0.9, 0.9}, "4"}
        });
        ASSERT(code == 0);

        auto closest = store.get_k_closest_hybrid_weighted(
            "doesn't matter", {0.8, 0.8}, 2, 1, DistanceMetric::cosine_similarity, 0
        );
        ASSERT(closest.size() == 2);
        ASSERT(closest[0].first == 0 && closest[0].second == "1");
        ASSERT(closest[0].first == 0 && closest[1].second == "4");

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/multi_weight_hybrid_search_test");

        std::cout << "TEST -- MULTI WEIGHT" << std::endl;
        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024, true);

        int code = add_embeddings(store, {
            {{1.0, 1.0}, "1 2"},
            {{0.0, 1.0}, "1 2"},
            {{1.0, 0.0}, "1 2"},
            {{0.9, 0.9}, "4"}
        });
        ASSERT(code == 0);

        auto closest = store.get_k_closest_hybrid_weighted(
            "1", {0.8, 0.8}, 3, 1, DistanceMetric::cosine_similarity, 0.8
        );
        ASSERT(closest.size() == 3);
        for(int i = 0 ; i < 3; i++)
            ASSERT(closest[i].second == "1 2");
        
        auto closest_2 = store.get_k_closest_hybrid_weighted(
            "1", {0.8, 0.8}, 2, 1, DistanceMetric::cosine_similarity, 0.2
        );
        ASSERT(closest_2.size() == 2);
        ASSERT(closest_2[0].second == "1 2");
        ASSERT(closest_2[1].second == "4");

        std::cout << "PASSED" << std::endl;
    }   

}