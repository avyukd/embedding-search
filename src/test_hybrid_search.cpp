#include <vector>
#include <iostream>

#include "embedding_store.h"
#include "test_utils.h"

int main(int argc, char** argv){
    {
        TestWrapper tw("test_files/basic_hybrid_search-test");

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

        auto closest = store.get_k_closest_hybrid_weighted(
            "v3", v1, 2, 1, DistanceMetric::cosine_similarity, 1
        );
        ASSERT(closest.size() == 2);
        PRINT_VEC_SECOND(closest);
        ASSERT(closest[0].second == "v1 v2 v3 v4");
        ASSERT(closest[1].second == "v3");
    }


}