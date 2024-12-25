#include <iostream>
#include "pugixml.hpp"
#include <unordered_map>
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

std::unordered_map<std::string,
                   std::pair<int, std::unordered_map<std::string, std::vector<int>>>> positional_index;

enum eComponents{
    POSITIONAL_INDEX,
    AUTO_COUNT
};
std::unordered_map<eComponents, bool> components_to_export;

void load_config_file()
{
    pugi::xml_document config_file;

    pugi::xml_parse_result result = config_file.load_file("config.xml");

    if (result) {
        std::cout << "config.xml successfully parsed!" << std::endl;

        pugi::xml_node root = config_file.child("config");
        for (pugi::xml_node file_node : root.children("file")) {
            pugi::xml_node name_node = file_node.child("name");
            if (name_node) {
                std::cout << "filename=" << name_node.text().as_string() << std::endl;
            } else {
                std::cout << "No <name> node found in <config><file>." << std::endl;
            }
        }

        pugi::xml_node components_node = root.child("components");
        if (components_node) {
            pugi::xml_node positional_index_node = components_node.child("positional_index");
            if (positional_index_node) {
                auto b_positional_index_export = positional_index_node.attribute("export").as_bool();
                components_to_export[POSITIONAL_INDEX] = b_positional_index_export;
            } else {
                std::cout << "<positional_index> node not found." << std::endl;
            }
        } else {
            std::cout << "<components> node not found." << std::endl;
        }

    } else {
        std::cout << "Unable to load config.xml: " << result.description() << std::endl;
    }
    std::cout << std::endl;
}

bool isCyrillicLetter(char32_t ch) {
    return (ch >= 0x0410 && ch <= 0x044F) || ch == 0x0401 || ch == 0x0451;
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
        cyrillicWordToLowercase(word);
        auto& [doc_frequency, postings_map] = positional_index[word];

        if (postings_map.find(filepath) == postings_map.end()) {
            doc_frequency++;
        }

        postings_map[filepath].push_back(position);
        position++;
    }

    file.close();
}

void read_file(const std::string& filepath)
{
    #ifdef _WIN32
    system("chcp 65001 > nul");
    #endif

    std::ifstream file(filepath);
    std::string line;

    if (file.is_open()) {
        while (getline(file, line)) {
            //std::cout << line << std::endl;
            add_to_index(filepath);
        }
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
        std::cout << "Invalid directory path: " << directory_path << std::endl;
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
                //if(directory_path=="resources\\BG_texts\\texts\\Sport" && file_or_dir_name=="text_001.txt"){
                if(file_or_dir_name=="text_test.txt" || file_or_dir_name=="text_test_2.txt") {
                    std::cout << "Reading file: " << directory_path + "\\" + file_or_dir_name << std::endl;
                    read_file(directory_path + "\\" + file_or_dir_name);
                }
            }
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
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
        }
    }

    if (doc.save_file("positional_index.xml")) {
        std::cout << "Positional index successfully written to 'positional_index.xml'.\n";
    } else {
        std::cerr << "Failed to write positional index to 'positional_index.xml'.\n";
    }
}

int main()
{
    load_config_file();

    const std::string& root_directory = "resources\\BG_texts\\texts";
    traverse_directory(root_directory);
    std::cout << std::endl;

    if(components_to_export.at(POSITIONAL_INDEX)) write_positional_index_to_xml();

    return 0;
}
