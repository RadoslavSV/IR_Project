#include <iostream>
#include <sys/stat.h>
#include "pugixml.hpp"
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <windows.h>
#include <sstream>
#include <locale>
#include <codecvt>
#include <set>
#include <cctype>
#include <algorithm>
#include "BTrie.h"

/////////////////////////////////////////////////////////
std::string resourse_dir_path = "resources";

enum eComponents{
    INVERTED_INDEX,
    POSITIONAL_INDEX,
    B_TRIE,
    AUTO_COUNT
};
std::unordered_map<eComponents, bool> components_to_export;

std::map<std::string,
         std::pair<int, std::unordered_map<std::string, std::vector<int>>>> positional_index;

BTrie b_trie;
/////////////////////////////////////////////////////////

bool directory_exists(const std::string& dir_name) {
    struct stat info;

    if (stat(dir_name.c_str(), &info) == 0 && (info.st_mode & S_IFDIR)) {
        return true;
    } else {
        return false;
    }
}

void load_config_file()
{
    pugi::xml_document config_file;

    pugi::xml_parse_result result = config_file.load_file("config.xml");

    if (result) {
        std::cout << "config.xml successfully parsed!" << std::endl;

        pugi::xml_node root = config_file.child("config");
        for (pugi::xml_node file_node : root.children("resource_directory")) {
            pugi::xml_node name_node = file_node.child("name");
            if (name_node) {
                resourse_dir_path = name_node.text().as_string();
                std::cout << "resourse_dir_path = " << resourse_dir_path << std::endl;
            } else {
                std::cerr << "No <name> node found in <config><resource_directory>." << std::endl;
            }
        }

        pugi::xml_node components_node = root.child("components");
        if (components_node) {
            pugi::xml_node inverted_index_node = components_node.child("inverted_index");
            if (inverted_index_node) {
                auto b_inverted_index_export = inverted_index_node.attribute("export").as_bool();
                components_to_export[INVERTED_INDEX] = b_inverted_index_export;
            } else {
                std::cerr << "<inverted_index> node not found." << std::endl;
            }

            pugi::xml_node positional_index_node = components_node.child("positional_index");
            if (positional_index_node) {
                auto b_positional_index_export = positional_index_node.attribute("export").as_bool();
                components_to_export[POSITIONAL_INDEX] = b_positional_index_export;
            } else {
                std::cerr << "<positional_index> node not found." << std::endl;
            }

            pugi::xml_node b_trie_node = components_node.child("b_trie");
            if (b_trie_node) {
                auto b_b_trie_export = b_trie_node.attribute("export").as_bool();
                components_to_export[B_TRIE] = b_b_trie_export;
            } else {
                std::cerr << "<b_trie> node not found." << std::endl;
            }

        } else {
            std::cerr << "<components> node not found." << std::endl;
        }

    } else {
        std::cerr << "Unable to load config.xml: " << result.description() << std::endl;
    }
    std::cout << std::endl;
}

bool isCyrillicLetter(char32_t ch) {
    return (ch >= 0x0410 && ch <= 0x044F) || ch == 0x0401 || ch == 0x0451;
}

bool isCyrillicWord(const std::string& word) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string utf32input = converter.from_bytes(word);
    return std::all_of(utf32input.begin(), utf32input.end(), isCyrillicLetter);
}

void cleanCyrillicWord(std::string& input) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string utf32input = converter.from_bytes(input);

    auto start = std::find_if(utf32input.begin(), utf32input.end(), isCyrillicLetter);
    auto end = std::find_if(utf32input.rbegin(), utf32input.rend(), isCyrillicLetter).base();

    if (start >= end) {
        input = "";
        return;
    }

    std::u32string cleanWord(start, end);

    input = converter.to_bytes(cleanWord);
}

void cyrillicWordToLowercase(std::string& word) {
    for(auto itr = word.begin()+1; itr != word.end()+1; itr++, itr++) {
        if(*itr>=-112 && *itr<=-97) {
            *itr += 32;
        } else if(*itr>=-96 && *itr<=-81) {
            *(itr-1) += 1;
            *itr -= 32;
        }
    }
}

void add_to_index(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filepath << std::endl;
        return;
    }

    std::string word;
    int position = 0;
    while (file >> word) {
        cleanCyrillicWord(word);
        if(isCyrillicWord(word)) {
            cyrillicWordToLowercase(word);
        }

        if(word.size() == 0) continue;

        auto& [doc_frequency, postings_map] = positional_index[word];

        if (postings_map.find(filepath) == postings_map.end()) {
            doc_frequency++;
        }

        postings_map[filepath].push_back(position);
        position++;

        if(components_to_export.at(B_TRIE) && isCyrillicWord(word)) {
            b_trie.insert(word);
        }
    }

    file.close();
}

void read_file(const std::string& filepath)
{
    #ifdef _WIN32
    system("chcp 65001 > nul");
    #endif

    std::ifstream file(filepath);

    if (file.is_open()) {
        add_to_index(filepath);
        file.close();
    } else {
        std::cout << "Could not open file: " << filepath << std::endl;
    }
}

void traverse_directory(const std::string& directory_path)
{
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile((directory_path + "\\*").c_str(), &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cerr << "Invalid directory path: " << directory_path << std::endl;
        return;
    }

    do {
        const std::string file_or_dir_name = findFileData.cFileName;

        if (file_or_dir_name == "." || file_or_dir_name == "..") {
            continue;
        }

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            traverse_directory(directory_path + "\\" + file_or_dir_name);
        } else {
            if (file_or_dir_name.substr(file_or_dir_name.size() - 4) == ".txt") {
            /// READ ONLY ONE/TWO FILES FOR DEVELOPMENT
                if(file_or_dir_name=="text_test.txt" || file_or_dir_name=="text_test_2.txt") {
                    std::cout << "Reading file: " << directory_path + "\\" + file_or_dir_name << std::endl;
                    read_file(directory_path + "\\" + file_or_dir_name);
                }
            }
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
}

void write_inverted_index_to_xml()
{
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("inverted_index");

    for (const auto& [term, term_data] : positional_index) {
        const auto& [doc_frequency, postings_map] = term_data;

        pugi::xml_node term_node = root.append_child("term");
        term_node.append_attribute("value") = term.c_str();

        for (const auto& [filepath, positions] : postings_map) {
            pugi::xml_node doc_node = term_node.append_child("document");
            doc_node.append_attribute("path") = filepath.c_str();
        }
    }

    if (mkdir("output") && errno != EEXIST) {
        std::cerr << "Error creating output directory\n";
    }

    const std::string file_path = "output\\inverted_index.xml";
    if (doc.save_file(file_path.c_str())) {
        std::cout << "Inverted index successfully written to " << file_path << std::endl;
    } else {
        std::cerr << "Failed to write inverted index to " << file_path << std::endl;
    }
}

void write_positional_index_to_xml()
{
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("positional_index");

    for (const auto& [term, term_data] : positional_index) {
        const auto& [doc_frequency, postings_map] = term_data;

        pugi::xml_node term_node = root.append_child("term");
        term_node.append_attribute("value") = term.c_str();
        term_node.append_attribute("doc_frequency") = doc_frequency;

        for (const auto& [filepath, positions] : postings_map) {
            pugi::xml_node doc_node = term_node.append_child("document");
            doc_node.append_attribute("path") = filepath.c_str();

            std::string allPos = "{";
            for (int pos : positions) {
                allPos += std::to_string(pos) + ", ";
            }
            allPos.at(allPos.size()-2) = '}';
            allPos.pop_back();
            pugi::xml_node pos_node = doc_node.append_child("positions");
            pos_node.text() = allPos.c_str();
            pos_node.append_attribute("term_frequency") = std::to_string(positions.size()).c_str();
        }
    }

    if (mkdir("output") && errno != EEXIST) {
        std::cerr << "Error creating output directory\n";
    }

    const std::string file_path = "output\\positional_index.xml";
    if (doc.save_file(file_path.c_str())) {
        std::cout << "Positional index successfully written to " << file_path << std::endl;
    } else {
        std::cerr << "Failed to write positional index to " << file_path << std::endl;
    }
}

int main()
{
    for(eComponents comp = eComponents::INVERTED_INDEX; comp < eComponents::AUTO_COUNT; comp = eComponents(comp + 1)) {
        components_to_export[comp] = false;
    }

    load_config_file();
    if (!directory_exists(resourse_dir_path)) {
        std::cerr << "Directory \'" << resourse_dir_path << "\' does not exist: " << std::endl;
        return 1;
    }

    traverse_directory(resourse_dir_path);
    std::cout << std::endl;

    if(components_to_export.at(INVERTED_INDEX))      write_inverted_index_to_xml();
    if(components_to_export.at(POSITIONAL_INDEX))    write_positional_index_to_xml();
    if(components_to_export.at(B_TRIE))              b_trie.exportToXML("output\\b_trie.xml");

    return 0;
}
