#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include "trie.h"

using namespace dutil;

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cout << argv[0] << ": FILE [1|2]" << std::endl;
        return 0;
    }

    std::ifstream source(argv[1]);
    trie *trie = trie::create_trie(atoi(argv[2]) == 1?SINGLE_TRIE:DOUBLE_TRIE);
    key_type key;
    struct timezone tz;
    struct timeval total = {0, 0}, tv[2];

    std::cerr.precision(15);
    if (source.is_open()) {
        std::string line;
        int i = 0;

        while (!source.eof()) {
            getline(source, line);
            if (line.empty()) continue;

            key.assign(line.c_str(), line.length());
            gettimeofday(&tv[0], &tz);
            trie->insert(key, i + 1);
            gettimeofday(&tv[1], &tz);
            total.tv_sec += tv[1].tv_sec - tv[0].tv_sec;
            total.tv_usec += tv[1].tv_usec - tv[0].tv_usec;
            while (total.tv_usec > 1000000) {
                total.tv_usec -= 1000000;
                total.tv_sec++;
            }
            ++i;
        }
        std::cerr << i << " items loaded." << std::endl;
        std::cerr << "total insertion time = "
            << (double)total.tv_sec * 1000.0 + total.tv_usec / 1000.0
            << "ms, average insertion time = "
            << (double)(total.tv_sec * 1000000.0 + total.tv_usec) / i
            << "us" << std::endl;
    }

    memset(&total, 0, sizeof(total));
    std::ifstream check(argv[1]);
    if (check.is_open()) {
        std::string line;
        int i = 0, j = 0;
        while (!check.eof()) {
            value_type value;
            getline(check, line);
            if (line.empty()) continue;
            gettimeofday(&tv[0], &tz);
            key.assign(line.c_str(), line.length());
            if (trie->search(key, &value)) {
                gettimeofday(&tv[1], &tz);
                total.tv_sec += (tv[1].tv_sec - tv[0].tv_sec);
                total.tv_usec += (tv[1].tv_usec - tv[0].tv_usec);
                while (total.tv_usec > 1000000) {
                    total.tv_usec -= 1000000;
                    total.tv_sec++;
                }
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
        std::cerr << "total searching time = "
            << (double)(total.tv_sec * 1000 + total.tv_usec) / 1000.0
            << "ms, average searching time = "
            << (double)(total.tv_sec * 1000000.0 + total.tv_usec) / j
            << "us" << std::endl;
    }

    delete trie;

    return 0;
}

// vim: ts=4 sw=4 ai et
