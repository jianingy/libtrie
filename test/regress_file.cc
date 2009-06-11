#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include "trie.h"

using namespace trie;

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cout << argv[0] << ": FILE [1|2]" << std::endl;
        return 0;
    }

    std::ifstream source(argv[1]);
    trie_interface *trie = create_trie(atoi(argv[2]) == 1?SINGLE_TRIE:DOUBLE_TRIE);
    key_type key;

    if (source.is_open()) {
        std::string line;
        int i = 0;

        while (!source.eof()) {
            getline(source, line);
            if (line.empty()) continue;

            key.assign(line.c_str(), line.length());
            trie->insert(key, i + 1);
            ++i;
        }
        std::cerr << i << " items loaded." << std::endl;
    }

    std::ifstream check(argv[1]);
    if (check.is_open()) {
        std::string line;
        int i = 0, j = 0;
        while (!check.eof()) {
            value_type value;
            getline(check, line);
            if (line.empty()) continue;
            key.assign(line.c_str(), line.length());
            if (trie->search(key, &value)) {
                std::cout << value << " " << line.c_str() << std::endl;
                ++j;
            } else {
                std::cerr << "lose '" << line << "' ret = "
                          << value
                          << std::endl;
            }
            ++i;
        }
        std::cerr << i << " items reviewed. " << j << " items stored" << std::endl;
    }

    delete trie;

    return 0;
}

// vim: ts=4 sw=4 ai et
