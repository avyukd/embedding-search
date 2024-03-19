#include <cmath>
#include <stdexcept>

#include "utils.h"

enum class DistanceMetric {
    cosine_similarity,
    manhattan,
    l2_squared
};


float compute_cosine_similarity_distance(float* embedding1, char* embedding2, uint32_t embedding_size){
    uint32_t float_size = sizeof(float);
    
    float dot_product = 0;
    float norm1 = 0;
    float norm2 = 0;

    for(uint32_t i = 0; i < embedding_size; i++){
        float f1 = *(embedding1 + i);
        float f2 = char_to_float(embedding2 + i * float_size, float_size);

        dot_product += f1 * f2;
        norm1 += f1 * f1;
        norm2 += f2 * f2; 
    }
    return 1 - dot_product / (sqrt(norm1) * sqrt(norm2));
}

float compute_manhattan_distance(float* embedding1, char* embedding2, uint32_t embedding_size, float thres){
    // thres for early cutoff
    return 0;
}

float compute_l2_squared_distance(float* embedding1, char* embedding2, uint32_t embedding_size, float thres){
    return 0;
}


float compute_distance(float* embedding1, char* embedding2, uint32_t embedding_size, DistanceMetric metric, float thres = 0){
    switch(metric){
        case DistanceMetric::cosine_similarity:
            return compute_cosine_similarity_distance(embedding1, embedding2, embedding_size);
        case DistanceMetric::manhattan:
            return compute_manhattan_distance(embedding1, embedding2, embedding_size, thres);
        case DistanceMetric::l2_squared:
            return compute_l2_squared_distance(embedding1, embedding2, embedding_size, thres);
        default:
            throw std::invalid_argument("Invalid distance metric");
    }
}
