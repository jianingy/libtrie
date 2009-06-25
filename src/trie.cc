/*
 * Copyright (c) 2009, Jianing Yang<jianingy.yang@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The names of its contributors may not be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY detrox@gmail.com ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL detrox@gmail.com BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <sys/time.h>
#include <stdint.h>
#include <limits.h>

#include <iostream>
#include <cstdlib>
#include <cstdio>

#include "trie.h"
#include "trie_impl.h"

BEGIN_TRIE_NAMESPACE

static trie::trie_type find_archive_type(const char *archive)
{
    FILE *fp;
    char magic[16] = {0};
    if ((fp = fopen(archive, "r"))) {
        size_t length = fread(magic, 1, sizeof(magic) / sizeof(char) - 1, fp);
        fclose(fp);
        if (strncmp(magic, "TWO_TRIE", length) == 0)
            return trie::DOUBLE_TRIE;
        else if (strncmp(magic, "TAIL_TRIE", length) == 0)
            return trie::SINGLE_TRIE;
        else
            return trie::UNKNOW;
    } else {
        throw bad_trie_archive("file error");
    }
}

trie* trie::create_trie(trie_type type, size_t size)
{
    if (type == SINGLE_TRIE)
        return new single_trie(size);
    else
        return new double_trie(size);
}

trie* trie::create_trie(const char *archive)
{
    trie_type type = find_archive_type(archive);
    if (type  == SINGLE_TRIE)
        return new single_trie(archive);
    else if (type == DOUBLE_TRIE)
        return new double_trie(archive);
    else
        throw bad_trie_archive("file magic error");
}

void trie::insert(const char *inputs, size_t length,
                            value_type value)
{
    key_type key(inputs, length);
    insert(key, value);
}

bool trie::search(const char *inputs, size_t length,
                            value_type *value) const
{
    key_type key(inputs, length);
    return search(key, value);
}

void trie::read_from_text(const char *source, bool verbose)
{
    FILE *file;
    if ((file = fopen(source, "r"))) {
        char fmt[LINE_MAX];
        char cstr[LINE_MAX];
        int val;
        size_t lineno = 0;
        key_type key;
        struct timezone tz;
        struct timeval total = {0, 0}, tv[2];

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
            if (verbose)
                gettimeofday(&tv[0], &tz);
            key.assign(cstr, strlen(cstr));
            insert(key, val);
            if (verbose) {
                gettimeofday(&tv[1], &tz);
                total.tv_sec += tv[1].tv_sec - tv[0].tv_sec;
                total.tv_usec += tv[1].tv_usec - tv[0].tv_usec;
                while (total.tv_usec > 1000000) {
                    total.tv_usec -= 1000000;
                    total.tv_sec++;
                }
            }
        }
        if (verbose) {
            std::cerr.precision(15);
            std::cerr << "..." << lineno << "." << std::endl
                      << "total insertion time = "
                      << total.tv_sec * 1000.0 + total.tv_usec / 1000.0 << "ms "
                      << ", average insertion time = "
                      << (total.tv_sec * 1000000.0 + total.tv_usec) / lineno
                      << "us" << std::endl;
        }
        fclose(file);
    } else {
        throw bad_trie_source("file error");
    }
}

END_TRIE_NAMESPACE

// vim: ts=4 sw=4 ai et
