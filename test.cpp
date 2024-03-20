#include <vector>
#include <iostream>
#include <cassert>
#include <filesystem>

#include "embedding_store.h"

#define IGNORE_TEST if(0)
#define ASSERT(x) if(!(x)){tw.~TestWrapper();assert(x);}
#define PRINT_VEC(x) for(auto i : x){std::cout << i << " ";}

namespace fs = std::filesystem;

class TestWrapper {
    private:
        fs::path dir_path;
    public:
        TestWrapper(const std::string& dir) : dir_path(dir) {}
        ~TestWrapper(){
            auto n = fs::remove_all(dir_path);
            std::cout << "Removed " << n << " files at " << dir_path.string() << std::endl;
        }
        fs::path get_dir_path(){
            return dir_path;
        }
};  

void add_embeddings(EmbeddingStore& store, std::vector<std::pair<float*, std::string>> embeddings){
    for(auto [k, v] : embeddings){
        store.add_embedding(k, v);
    }
}

int main(int argc, char** argv){
    {
        TestWrapper tw("test_files/basic_test");

        std::cout << "TEST -- BASIC..." << std::endl;
        EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024);        
        
        float k1[2] = {1.0, 1.0}; std::string v1 = "v1";
        float k2[2] = {0.0, 1.0}; std::string v2 = "v2interesting";
        float k3[2] = {1.0, 0}; std::string v3 = "v3test";
        float k4[2] = {0.0, 0.0}; std::string v4 = "";
        float k5[2] = {0.9, 0.9}; std::string v5 = "v5&&0";

        add_embeddings(store, {
            {k1, v1}, {k2, v2}, {k3, v3}, {k4, v4}, {k5, v5}
        });

        std::vector<std::string> closest = store.get_k_closest(k1, 2, 1, DistanceMetric::cosine_similarity);
        ASSERT(closest.size() == 2);
        ASSERT(closest[0] == v1);
        ASSERT(closest[1] == v5);

        float k6[2] = {0.6, 0.5}; std::string v6 = "v6";
        store.add_embedding(k6, v6);

        float k7[2] = {1.2, 1};
        closest = store.get_k_closest(k7, 1, 1, DistanceMetric::cosine_similarity);
        ASSERT(closest[0] == v6);

        std::cout << "PASSED" << std::endl;
    }

    {
        TestWrapper tw("test_files/load_test");
        std::cout << "TEST -- LOAD..." << std::endl;
        float k2[2] = {0.2, 0.4}; std::string v2 = "basketball";
        {
            std::cout << "Add embeddings..." << std::endl;
            EmbeddingStore store(tw.get_dir_path().c_str(), 2, 1024, 1024);        
        
            float k1[2] = {0.2, 0.3}; std::string v1 = "horse";
            float k3[2] = {1.2, 2.5}; std::string v3 = "football";

            add_embeddings(store, {
                {k1, v1}, {k2, v2}, {k3, v3}
            });
        }
        {
            std::cout << "Load embeddings..." << std::endl;
            EmbeddingStore store(tw.get_dir_path().c_str(), 2);        

            float k1[2] = {0.2, 0.3};
            std::vector<std::string> closest = store.get_k_closest(k1, 3, 1, DistanceMetric::cosine_similarity);
            PRINT_VEC(closest);
            ASSERT(closest.size() == 3);
            ASSERT(closest[0] == "horse");
            ASSERT(closest[1] == "basketball");
            ASSERT(closest[2] == "football");

            std::cout << "Add more embeddings..." << std::endl;
            float k4[2] = {0.7, 0.7}; std::string v4 = "nascar";
            store.add_embedding(k4, v4);

            std::vector<std::string> closest_second = store.get_k_closest(k2, 1, 1, DistanceMetric::cosine_similarity);
            ASSERT(closest_second.size() == 1);
            ASSERT(closest_second[0] == "basketball");
        }
        std::cout << "PASSED" << std::endl;
    }


}