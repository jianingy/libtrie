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

using namespace dutil;

static void *
query_trie(const char *query, const char *index, bool prefix, bool verbose)
{
    int retval = 0;
    trie::value_type value;
    trie *mtrie = trie::create_trie(index);
    trie::key_type key(query, strlen(query));
    if (prefix) {
        trie::result_type result;
        mtrie->prefix_search(key, &result);
        trie::result_type::const_iterator it;
        for (it = result.begin(); it != result.end(); it++)
            std::cout << it->second << " " << it->first.c_str() << std::endl;
    } else {
        if (mtrie->search(key, &value)) {
            std::cout << value << std::endl;
        } else {
            std::cerr << query << " not found." << std::endl;
            retval = 1;
        }
    }
    delete mtrie;
    exit(retval);
}

static void *
build_trie(const char *source, const char *index, trie::trie_type type, bool verbose)
{
    trie *mtrie = trie::create_trie(type);
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
                 "        -p|--prefix           prefix mode query\n"
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
    trie::trie_type type = trie::DOUBLE_TRIE;
    bool verbose = false;
    bool prefix = false;
    bool dump = false;

    while (true) {
        static struct option long_options[] =
        {
            {"build", required_argument, 0, 'b'},
            {"dump", no_argument, 0, 'd'},
            {"help", no_argument, 0, 'h'},
            {"prefix", no_argument, 0, 'p'},
            {"query", required_argument, 0, 'q'},
            {"type", required_argument, 0, 't'},
            {"verbose", no_argument, 0, 'v'},
            {0, 0, 0, 0}
        };
        int option_index;

        c = getopt_long(argc, argv, "b:dhpq:t:v", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'h':
                help_message();
                return 0;
            case 'b':
                source = optarg;
                break;
            case 'd':
                dump = true;
                break;
            case 'p':
                prefix = true;
                break;
            case 'q':
                query = optarg;
                break;
            case 't':
                switch (atoi(optarg)) {
                    case 1:
                        type = trie::SINGLE_TRIE;
                        break;
                    case 2:
                        type = trie::DOUBLE_TRIE;
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
            query_trie(query, index, prefix, verbose);
        else if (dump)
            query_trie("", index, true, verbose);
    }
    help_message();


    return 0;
}

// vim: ts=4 sw=4 ai et
