// Copyright Jianing Yang <jianingy.yang@gmail.com>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "trie.h"

using namespace trie;

static void *
query_trie(const char *query, const char *index, bool verbose)
{
    int retval = 0;
    value_type value;
    trie_interface *mtrie = create_trie(index);
    if (mtrie->search(query, strlen(query), &value)) {
        std::cout << value << std::endl;
    } else {
        std::cerr << query << " not found." << std::endl;
        retval = 1;
    }
    delete mtrie;
    exit(retval);
}

static void *
build_trie(const char *source, const char *index, trie_type type, bool verbose)
{
    trie_interface *mtrie = create_trie(type);
    mtrie->read_from_text(source, verbose);
    if (verbose)
        std::cerr << "writing to disk..." << std::endl;
    mtrie->build(index, verbose);
    if (verbose)
        std::cerr << "done" << std::endl;
    delete mtrie;
    exit(0);
}

static void help_message()
{
    std::cout << "Usage: trie_tool [OPTIONS] archive\n"
                 "Utility to manage archive of libxtree \n"
                 "OPTIONS:\n"
                 "        -b|--build SOURCE     build from SOURCE\n"
                 "        -h|--help             help message\n"
                 "        -q|--query QUERY      lookup QUERY in archive\n"
                 "        -t|--type TYPE        archive type\n"
                 "        -v|--verbose          verbose\n\n"
                 "SOURCE FORMAT:\n"
                 "        value word\n\n"
                 "ARCHIVE TYPE:\n"
                 "        1: tail-trie\n"
                 "        2: two-trie (default value)\n"
                 "\n"
                 "Report bugs to jianing.yang@alibaba-inc.com\n"
              << std::endl;
}

int main(int argc, char *argv[])
{
    int c;
    const char *index = NULL, *source = NULL, *query = NULL;
    trie_type type = DOUBLE_TRIE;
    bool verbose = false;

    while (true) {
        static struct option long_options[] =
        {
            {"build", required_argument, 0, 'b'},
            {"help", no_argument, 0, 'h'},
            {"query", required_argument, 0, 'q'},
            {"type", required_argument, 0, 't'},
            {"verbose", no_argument, 0, 'v'},
            {0, 0, 0, 0}
        };
        int option_index;

        c = getopt_long(argc, argv, "b:hq:t:v", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'h':
                help_message();
                return 0;
            case 'b':
                source = optarg;
                break;
            case 'q':
                query = optarg;
                break;
            case 't':
                switch(atoi(optarg)) {
                    case 1:
                        type = SINGLE_TRIE;
                        break;
                    case 2:
                        type = DOUBLE_TRIE;
                        break;
                    default:
                        help_message();
                        exit(0);
                }
                break;
            case 'v':
                verbose = true;
                break;
        }
    }

    if (optind < argc) {
        index = argv[optind];
        if (source)
            build_trie(source, index, type, verbose);
        else if (query)
            query_trie(query, index, verbose);
    }
    help_message();


    return 0;
}

// vim: ts=4 sw=4 ai et
