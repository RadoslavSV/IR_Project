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
    USER_QUERY,
    EDIT_DISTANCE,
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
                components_to_export[INVERTED_INDEX] = inverted_index_node.attribute("export").as_bool();
            } else {
                std::cerr << "<inverted_index> node not found." << std::endl;
            }

            pugi::xml_node positional_index_node = components_node.child("positional_index");
            if (positional_index_node) {
                components_to_export[POSITIONAL_INDEX] = positional_index_node.attribute("export").as_bool();
            } else {
                std::cerr << "<positional_index> node not found." << std::endl;
            }

            pugi::xml_node b_trie_node = components_node.child("b_trie");
            if (b_trie_node) {
                components_to_export[B_TRIE] = b_trie_node.attribute("export").as_bool();
            } else {
                std::cerr << "<b_trie> node not found." << std::endl;
            }

            pugi::xml_node user_query_node = components_node.child("user_query");
            if (user_query_node) {
                components_to_export[USER_QUERY] = user_query_node.attribute("export").as_bool();
            } else {
                std::cerr << "<user_query> node not found." << std::endl;
            }

            pugi::xml_node edit_distance_node = components_node.child("edit_distance");
            if (edit_distance_node) {
                components_to_export[EDIT_DISTANCE] = edit_distance_node.attribute("export").as_bool();
            } else {
                std::cerr << "<edit_distance> node not found." << std::endl;
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
                std::cout << "Reading file: " << directory_path + "\\" + file_or_dir_name << std::endl;
                read_file(directory_path + "\\" + file_or_dir_name);
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

void user_query_results_to_xml(const std::string& query) {
    pugi::xml_document doc;

    pugi::xml_node user_query_node = doc.append_child("user_query");
    user_query_node.append_attribute("query") = query.c_str();

    std::string search_query = query;
    cleanCyrillicWord(search_query);
    if(isCyrillicWord(search_query)) {
        cyrillicWordToLowercase(search_query);
    }

    auto term_it = positional_index.find(search_query);
    if (term_it != positional_index.end()) {
        const auto& [doc_frequency, postings_map] = term_it->second;

        std::cout << "Query term \"" << query << "\" found in files:" << std::endl;
        for (const auto& [filepath, positions] : postings_map) {
            pugi::xml_node document_node = user_query_node.append_child("document");
            document_node.append_attribute("path") = filepath.c_str();
            std::cout << filepath << std::endl;

            pugi::xml_node positions_node = document_node.append_child("positions");
            positions_node.append_attribute("term_frequency") = static_cast<int>(positions.size());

            std::string all_positions = "{";
            for (int pos : positions) {
                all_positions += std::to_string(pos) + ", ";
            }
            all_positions.at(all_positions.size() - 2) = '}';
            all_positions.pop_back();
            positions_node.text() = all_positions.c_str();
        }
    } else {
        std::cerr << "Query term \"" << query << "\" not found in the index." << std::endl;
    }
    std::cout << std::endl;

    const std::string& output_path = "output\\user_query.xml";
    if (doc.save_file(output_path.c_str())) {
        std::cout << "Query results successfully exported to " << output_path << std::endl;
    } else {
        std::cerr << "Failed to export query results to " << output_path << std::endl;
    }
}

struct TermDistance {
    std::string term;
    int distance;
    std::vector<std::vector<int>> matrix;
};

std::u32string toUTF32(const std::string& utf8_str) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    return converter.from_bytes(utf8_str);
}

TermDistance compute_Levenshtein_distance(const std::string& query, const std::string& term) {
    if (!isCyrillicWord(query) || !isCyrillicWord(term)) {
        return {"",0,{}};
    }
    std::u32string utf32word1 = toUTF32(query);
    std::u32string utf32word2 = toUTF32(term);

    int m = utf32word1.size();
    int n = utf32word2.size();
    std::vector<std::vector<int>> matrix(m + 1, std::vector<int>(n + 1));

    for (int i = 0; i <= m; i++)
        matrix[i][0] = i;
    for (int j = 0; j <= n; j++)
        matrix[0][j] = j;

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int cost = (utf32word1[i - 1] == utf32word2[j - 1]) ? 0 : 1;
            matrix[i][j] = std::min({matrix[i - 1][j] + 1,
                                     matrix[i][j - 1] + 1,
                                     matrix[i - 1][j - 1] + cost
                                    });
        }
    }

    TermDistance result;
    result.term = term;
    result.distance = matrix[m][n];
    result.matrix = matrix;
    return result;
}

void edit_distance_to_xml(const std::string& query) {
    std::vector<TermDistance> results;

    for (const auto& [term, term_data] : positional_index) {
        TermDistance result = compute_Levenshtein_distance(query, term);
        if (result.distance >= 1 && result.distance <= 3) {
            results.push_back(result);
        }
    }

    std::sort(results.begin(), results.end(), [](const TermDistance& a, const TermDistance& b) {
        return a.distance < b.distance;
    });

    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("edit_distance");
    root.append_attribute("query") = query.c_str();

    for (const auto& result : results) {
        pugi::xml_node term_node = root.append_child("term");
        term_node.append_attribute("value") = result.term.c_str();
        term_node.append_attribute("levenshtein_distance") = result.distance;

        pugi::xml_node matrix_node = term_node.append_child("matrix");
        std::ostringstream matrix_stream;
        matrix_stream << "\n";
        for (const auto& row : result.matrix) {
            matrix_stream << "            ";
            for (int val : row) {
                matrix_stream << val << " ";
            }
            matrix_stream << "\n";
        }
        matrix_stream << "        ";
        matrix_node.text() = matrix_stream.str().c_str();
    }

    const std::string file_path = "output\\edit_distance.xml";
    if (doc.save_file(file_path.c_str())) {
        std::cout << "Edit distance successfully exported to " << file_path << std::endl;
    } else {
        std::cerr << "Failed to export edit distance!" << std::endl;
    }

    std::string search_query = query;
    cleanCyrillicWord(search_query);
    if(isCyrillicWord(search_query)) {
        cyrillicWordToLowercase(search_query);
    }
    auto term_it = positional_index.find(search_query);
    if (term_it == positional_index.end()) {
        bool bWritten = false;
        for (const auto& result : results) {
            if(result.distance == 1) {
                if(!bWritten) {
                    std::cout << "\nMaybe you meant: \n";
                    bWritten = true;
                }
                std::cout << result.term << std::endl;
            }
        }
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
    std::string query = "";
    if(components_to_export.at(USER_QUERY)) {
        std::cout << "\nEnter your query: ";
        std::cin >> query;
        user_query_results_to_xml(query);
    }
    if(components_to_export.at(EDIT_DISTANCE))       edit_distance_to_xml(query);

    return 0;
}
