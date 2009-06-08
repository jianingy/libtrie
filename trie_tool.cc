// Copyright Jianing Yang <jianingy.yang@gmail.com>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <iostream>
#include <stdexcept>

#include "trie.h"

static void *
query_trie(const char *query, const char *index, bool verbose)
{
    trie_interface *trie;
    int retval = 0;
    int value;
    int trie_type = 0;
    try {
        trie = new suffix_trie(index);
        trie_type = 1;
    } catch (...) {
        trie = new double_trie(index);
    }

    if (trie->search(query, strlen(query), &value)) {
        printf("%d\n", value);
    } else {
        fprintf(stderr, "%s not found\n", query);
        retval = 1;
    }

    if (trie_type == 1)
        delete static_cast<suffix_trie *>(trie);
    else
        delete static_cast<double_trie *>(trie);

    exit(retval);
}

static void *
build_trie(const char *source, const char *index, int trie_type, bool verbose)
{
    trie_interface *trie;
    if (trie_type == 1)
        trie = new suffix_trie();
    else
        trie = new double_trie();

    FILE *file;
    if ((file = fopen(source, "r"))) {
        char fmt[LINE_MAX];
        char key[LINE_MAX];
        int val;
        size_t lineno = 0;

        if (verbose)
            fprintf(stderr, "building");
        snprintf(fmt, LINE_MAX, "%%d %%%d[^\n] ", LINE_MAX);
        while (!feof(file)) {
            if (verbose && lineno > 0) {
                if (lineno % 500 == 0)
                    fprintf(stderr, ".");
                if (lineno % 1500 == 0)
                    fprintf(stderr, "%d", lineno);
            }
            ++lineno;
            if (fscanf(file, fmt, &val, key) != 2) {
                fprintf(stderr,
                       "build_trie: format error at line %d\n",
                       lineno);
                exit(-1);
            }
            trie->insert(key, strlen(key), val);
        }
        if (verbose)
            fprintf(stderr, "...%d...saving", lineno);
        trie->build(index);
        if (verbose)
            fprintf(stderr, "...done\n");
    } else {
        perror("build_trie");
        exit(-1);
    }

    if (trie_type == 1)
        delete static_cast<suffix_trie *>(trie);
    else
        delete static_cast<double_trie *>(trie);

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
                 "        0: two-trie (default value)\n"
                 "        1: tail-trie\n"
                 "\n"
                 "Report bugs to jianing.yang@alibaba-inc.com\n"
              << std::endl;
}

int main(int argc, char *argv[])
{
    int c;
    const char *index = NULL, *source = NULL, *query = NULL;
    int trie_type = 0;
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
                trie_type = atoi(optarg);
                break;
            case 'v':
                verbose = true;
                break;
        }
    }

    if (optind < argc) {
        index = argv[optind];
        if (source)
            build_trie(source, index, trie_type, verbose);
        else if (query)
            query_trie(query, index, verbose);
    }
    help_message();


    return 0;
}

// vim: ts=4 sw=4 ai et
