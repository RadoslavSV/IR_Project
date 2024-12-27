#include "BTrie.h"

inline bool isCyrillicChar(char32_t ch) {
    return (ch >= 0x0410 && ch <= 0x044F) || ch == 0x0401 || ch == 0x0451;
}

BTrie::BTrie() {
    root = new BTrieNode(u'\0');
}

void BTrie::writeNodeToXML(pugi::xml_node& parent_xml_node, BTrieNode* node) {
    pugi::xml_node current_xml_node = parent_xml_node.append_child("node");
    current_xml_node.append_attribute("value") = std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t>().to_bytes(node->value).c_str();
    current_xml_node.append_attribute("end_of_word") = node->is_end_of_word;

    for (auto& child : node->children) {
        writeNodeToXML(current_xml_node, child.second);
    }
}

void BTrie::insert(const std::string& word) {
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
    std::u16string utf16word = converter.from_bytes(word);

    BTrieNode* current = root;
    for (char16_t ch : utf16word) {
        if (!isCyrillicChar(ch)) continue;

        if (current->children.find(ch) == current->children.end()) {
            current->children[ch] = new BTrieNode(ch);
        }
        current = current->children[ch];
    }
    current->is_end_of_word = true;
}

void BTrie::exportToXML(const std::string& filepath) {
    pugi::xml_document doc;
    pugi::xml_node root_xml_node = doc.append_child("BTrie");

    for (auto& child : root->children) {
        writeNodeToXML(root_xml_node, child.second);
    }

    if (doc.save_file(filepath.c_str())) {
        std::cout << "B-Trie successfully written to " << filepath << std::endl;
    } else {
        std::cerr << "Failed to export B-Trie to " << filepath << std::endl;
    }
}
