#pragma once
#include <vector>
#include <string>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <cstring> 
#include <fstream>

#include "constants.h"

std::string remove_punctuation(std::string value){
    std::string clean_value; clean_value.reserve(value.size());
    std::copy_if(value.begin(), value.end(), std::back_inserter(clean_value), [](char c){
        return std::isalnum(c) || c == ' ';
    });
    return clean_value;
}

std::vector<std::string> get_clean_words(const std::string& value){
    std::string lower_value; lower_value.reserve(value.size());
    std::transform(value.begin(), value.end(), std::back_inserter(lower_value), ::tolower);
    std::string clean_value = remove_punctuation(lower_value);

    std::vector<std::string> words;
    std::string word;
    for(char c : clean_value){
        if(c == ' '){
            if(word.size() > 0){
                words.push_back(word);
                word.clear();
            }
        }else{
            word.push_back(c);
        }
    }

    if(word.size() > 0){
        words.push_back(word);
    }

    return words;
}

std::vector<std::string> remove_common_words(std::vector<std::string> words){
    std::vector<std::string> common_words;
    std::ifstream common_words_file(COMMON_WORDS_FN);

    std::string common_word;
    while(common_words_file >> common_word){
        common_words.push_back(common_word);
    }

    std::vector<std::string> keys;
    for(std::string word : words){
        if(std::find(common_words.begin(), common_words.end(), word) == common_words.end()){
            keys.push_back(word);
        }
    }

    return keys;
}

std::vector<std::string> get_keys_from_string(std::string value){
    std::vector<std::string> words = get_clean_words(value);
    std::vector<std::string> keys = remove_common_words(words);
    return keys;
}