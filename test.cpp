#include <vector>
#include <iostream>
#include <cassert>

#include "embedding_store.h"


#define ENABLE_TEST 1
#define IGNORE_TEST 0

int main(int argc, char** argv){
    if(ENABLE_TEST){
        std::cout << "TEST -- BASIC..." << std::endl;
        EmbeddingStore store("test_files/basic_test", 2, 1024, 1024);        
        
        float k1[2] = {1.0, 1.0}; std::string v1 = "v1";
        float k2[2] = {0.0, 1.0}; std::string v2 = "v2interesting";
        float k3[2] = {1.0, 0}; std::string v3 = "v3test";
        float k4[2] = {0.0, 0.0}; std::string v4 = "";
        float k5[2] = {0.9, 0.9}; std::string v5 = "v5&&0";

        for(
            auto [k, v] : 
                std::initializer_list<std::pair<float*, std::string>>{
                    {k1, v1}, {k2, v2}, {k3, v3}, {k4, v4}, {k5, v5}
                }
        ){
            store.add_embedding(k, v);
        }

        std::vector<std::string> closest = store.get_k_closest(k1, 1, 1, DistanceMetric::cosine_similarity);
        assert(closest.size() == 1);
        std::cout << closest[0] << std::endl;
        assert(closest[0] == "v1");

        // assert(closest[1][0] == 0.9f);
        // assert(closest[1][1] == 0.9f);

        // float v6[2] = {0.6, 0.5};
        // store.add_embedding(v6);

        // float v7[2] = {1.2, 1};
        // closest = store.get_k_closest(v7, 2, 1, 1, DistanceMetric::cosine_similarity);
        // std::cout << closest[0][0] << " " << closest[0][1] << std::endl;

        // assert(closest.size() == 1);
        // assert(closest[0][0] == 0.6f);
        // assert(closest[0][1] == 0.5f);

        std::cout << "PASSED" << std::endl;
    }


}