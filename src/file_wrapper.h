#pragma once
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

#include "constants.h"
#include "utils.h"

namespace fs = std::filesystem;

struct FileWrapper {
    int fd = -1; 
    char* mmap_addr = nullptr;
    uint32_t write_idx = DEFAULT_WRITE_IDX;
    uint32_t mmap_size = 0;

    FileWrapper() = default;

    void init(const fs::path& path, uint32_t max_size){
        bool exists = fs::exists(path);
        mmap_size = exists ? fs::file_size(path) : max_size;
        // std::cout << "mmap_size: " << mmap_size << std::endl;
        // std::cout << "exists: " << exists << std::endl;

        const char* path_c_str = path.c_str();
        
        fd = open(path_c_str, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        assert(fd != -1);

        int alloc = ftruncate(fd, mmap_size);
        assert(alloc != -1);

        mmap_addr = (char*) mmap(
            NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        assert(mmap_addr != MAP_FAILED);

        if(!exists){
            memset(mmap_addr, 0, mmap_size);
        }else{
            write_idx = char_to_uint32_t(mmap_addr);
        }
    }

    void set_write_idx(){
        memcpy(mmap_addr, &write_idx, DEFAULT_WRITE_IDX);
    }

    ~FileWrapper(){
        set_write_idx();
        close(fd);
    }

    int write(const void* data, uint32_t size){
        if(write_idx + size > mmap_size){
            // std::cout << "write_idx: " << write_idx << " size: " << size << " mmap_size: " << mmap_size << std::endl;
            return -1;
        }
        memcpy(mmap_addr + write_idx, data, size);
        write_idx += size;
        return 0;
    }
};