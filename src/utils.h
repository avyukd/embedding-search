#pragma once
#include <stdint.h>
#include <string.h>
#include <filesystem>
#include <vector>
#include <iostream>

namespace fs = std::filesystem;

float char_to_float(char* c, uint32_t len){
    float f;
    memcpy(&f, c, len);
    return f;
}

uint32_t char_to_uint32_t(char* c){
    uint32_t u;
    memcpy(&u, c, sizeof(uint32_t));
    return u;
}

int get_num_paths_exist(const std::vector<fs::path> paths){
    int num_exist = 0;
    for(auto p : paths){
        num_exist += fs::exists(p);
    }
    return num_exist;
}