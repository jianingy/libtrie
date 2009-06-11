#include <stdint.h>
#include <limits.h>

#include <iostream>
#include <cstdlib>
#include <cstdio>

#include "trie.h"
#include "trie_impl.h"

BEGIN_TRIE_NAMESPACE

static trie_type find_archive_type(const char *archive)
{
    FILE *fp;
    char magic[16] = {0};
    if ((fp = fopen(archive, "r"))) {
        fread(magic, sizeof(magic) / sizeof(char) - 1, 1, fp);
        fclose(fp);
        if (strcmp(magic, "TWO_TRIE") == 0)
            return DOUBLE_TRIE;
        else if (strcmp(magic, "TAIL_TRIE") == 0)
            return SINGLE_TRIE;
        else
            return UNKNOW;
    } else {
        throw bad_trie_archive("file error");
    }
}

trie_interface *create_trie(trie_type type, size_t size)
{
    if (type == SINGLE_TRIE)
        return new single_trie(size);
    else
        return new double_trie(size);
}

trie_interface *create_trie(const char *archive)
{
    trie_type type = find_archive_type(archive);
    if (type  == SINGLE_TRIE)
        return new single_trie(archive);
    else if (type == DOUBLE_TRIE)
        return new double_trie(archive);
    else
        throw bad_trie_archive("file magic error");
}

void trie_interface::insert(const char *inputs, size_t length,
                            value_type value)
{
    insert(key_type(inputs, length), value);
}

bool trie_interface::search(const char *inputs, size_t length,
                            value_type *value) const
{
    return search(key_type(inputs, length), value);
}

void trie_interface::read_from_text(const char *source, bool verbose)
{
    FILE *file;
    if ((file = fopen(source, "r"))) {
        char fmt[LINE_MAX];
        char cstr[LINE_MAX];
        int val;
        size_t lineno = 0;
        key_type key;

        if (verbose)
            std::cerr <<  "building";
        snprintf(fmt, LINE_MAX, "%%d %%%d[^\n] ", LINE_MAX);
        while (!feof(file)) {
            if (verbose && lineno > 0) {
                if (lineno % 500 == 0)
                    std::cerr << ".";
                if (lineno % 1500 == 0)
                    std::cerr << lineno;
            }
            ++lineno;
            if (fscanf(file, fmt, &val, cstr) != 2) {
                if (verbose)  {
                    std::cerr << "build_trie: format error at line "
                              << lineno
                              << std::endl;
                }
                throw new bad_trie_source("format error");
            }
            key.assign(cstr, strlen(cstr));
            insert(key, val);
        }
        if (verbose)
            std::cerr << "..." << lineno << "." << std::endl;
        fclose(file);
    } else {
        throw bad_trie_source("file error");
    }
}

END_TRIE_NAMESPACE

// vim: ts=4 sw=4 ai et
