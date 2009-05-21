#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include "trie.h"

int main()
{
    basic_trie trie, trie2;
#define test_insert(str,val) do {                  \
        trie.insert(str, strlen(str) + 1, val);                  \
        printf("%s = %d\n", str, trie.search(str, strlen(str) + 1));   \
    } while(0);

#define test_search(str) do {                  \
        printf("%s = %d\n", str, trie.search(str, strlen(str) + 1));   \
    } while(0);
#if 1
    test_insert("baby", 1);
    test_insert("bachelor", 2);
    test_insert("back", 3);
    test_insert("badge", 4);
    test_insert("badger", 5);
    test_insert("badness", 6)
    test_insert("bcs", 7);
    //test_insert("×", 100);
    //test_insert("××", 101);
    //test_insert("°", 102);
    //test_insert("·", 103);
    //test_insert("ＦｏｒｄｈａｍＵｎｉｖｅｒｓｉｔｙ", 104);
    test_search("b");
    test_search("bbbb");

    trie.trace(1);
    printf("done %d\n", trie.go_backward(127, "baby", 4, NULL));
    printf("done %d\n", trie.go_backward(23, "badger", 7, NULL));
    trie2 = trie;
    trie2.insert("bcm", 4, 8);
    trie2.trace(1);
#else
    std::ifstream in("list");

    if (in.is_open()) {
        std::string line;
        int i = 0;

        while (!in.eof()) {
            getline(in, line);
            if (line.empty()) continue;

            trie.insert(line.c_str(), line.length() + 1, ++i);
//            std::cerr << "insert " << line << " = " << i << std::endl;
        }
        std::cerr << i << " items loaded." << std::endl;
    }

    std::ifstream in2("list");
    if (in2.is_open()) {
        std::string line;
        int i = 0;
        while (!in2.eof()) {
            getline(in2, line);
            if (line.empty()) continue;
//            std::cout << line.c_str() << " = " << trie.search(line.c_str()) << std::endl;
            if (trie.search(line.c_str(), line.length() + 1) > 0) {
                std::cout << line.c_str() << std::endl;
            }            
            ++i;
        }
        std::cerr << i << " items reviewed." << std::endl;    
    }
//    trie.trace(1);
#endif

    return 0;
}

// vim: ts=4 sw=4 ai et
