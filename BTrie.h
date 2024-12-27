#pragma once

#include <iostream>
#include <map>
#include <string>
#include <locale>
#include <codecvt>
#include "pugixml.hpp"

class BTrieNode {
public:
    char16_t  value;
    bool is_end_of_word;
    std::map<char16_t, BTrieNode*> children;

    BTrieNode(char16_t val) : value(val), is_end_of_word(false) {}
};

class BTrie {
private:
    BTrieNode* root;

    void writeNodeToXML(pugi::xml_node& parent_xml_node, BTrieNode* node);

public:
    BTrie();

    void insert(const std::string& word);

    void exportToXML(const std::string& filepath);
};
