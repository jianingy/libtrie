// Copyright Jianing Yang <detrox@gmail.com> 2009
#ifndef TRIE_H_
#define TRIE_H_

/*
 * Reference:
 * [1] J.AOE An Efficient Digital Search Algorithm by 
 *           Using a Double-Array Structure
 *
 * 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <stdexcept>
#include <vector>
#include <cassert>

class basic_trie
{
  public:
    typedef int32_t value_type;
    typedef int32_t size_type;
    typedef int32_t char_type;

    typedef struct {
        value_type base;
        value_type check;
    } state_type;

    typedef struct {
        char magic[28];
        size_type size;
    } header_type;

    typedef struct {
        char_type max;
        char_type min;
    } extremum_type;

    static const char_type kCharsetSize = 257;
    static const char_type kTerminator = kCharsetSize;

    explicit basic_trie(size_type size = 1024);
    basic_trie(const basic_trie &trie);
    void clone(const basic_trie &trie);
    basic_trie &operator=(const basic_trie &trie);
    ~basic_trie();
    void insert(const char *inputs, size_t length, value_type val);
    value_type search(const char *inputs, size_t length) const;
    void trace(size_type s) const;
    size_type create_link(size_type s, char_type ch);

    static const basic_trie *create_from_memory(void *header,
                                                void *states)
    {
        return new basic_trie(header, states);
    }

    value_type base(size_type s) const
    {
        return states_[s].base;
    }

    value_type check(size_type s) const
    {
        return states_[s].check;
    }

    void set_base(size_type s, value_type val)
    {
        states_[s].base = val;
    }

    void set_check(size_type s, value_type val)
    {
        states_[s].check = val;
    }

    // Get next state from s with input ch
    size_type next(size_type s, char_type ch) const
    {
        return base(s) + ch;
    }

    // Get prev state from s with input ch
    size_type prev(size_type s) const
    {
        return check(s);
    }

    // Go forward from state s with inputs, returns the last
    // states and the mismatch position by pointer
    size_type go_forward(size_type s,
                         const char *inputs,
                         size_t length,
                         const char **mismatch) const
    {
        const char *p;
        for (p = inputs; p < inputs + length; p++) {
            char_type ch = char_in(*p);
            size_type t = next(s, ch);
            if (!check_transition(s, t))
                break;
            s = t;
        }
        if (mismatch)
            *mismatch = p;
        return s;
    }

    // Go backward from state s with inputs, returns the last
    // states and the mismatch position by pointer
    size_type go_backward(size_type s,
                          const char *inputs,
                          size_t length,
                          const char **mismatch) const
    {
        const char *p;
        for (p = inputs + length - 1; p >= inputs; p--) {
            char_type ch = char_in(*p);
            size_type t = prev(s);
            if (!check_transition(t, next(t, ch)))
                break;
            s = t;
        }
        if (mismatch)
            *mismatch = p;

        return s;
    }

    // Find out all exists targets from s and store them into *targets.
    // If max is not null, the maximum char makes state transit from s to
    // those targets will be stored into max. Same to min.
    size_type find_exist_target(size_type s,
                                char_type *targets,
                                extremum_type *extremum) const
    {
        char_type ch;
        char_type *p;

        for (ch = 1, p = targets; ch < kCharsetSize + 1; ch++) {
            size_type t = next(s, ch);
            if (t >= header_->size)
                break;
            if (check_transition(s, t)) {
                *(p++) = ch;
                if (extremum) {
                    if (ch > extremum->max)
                        extremum->max = ch;
                    if (ch < extremum->min)
                        extremum->min = ch;
                }
            }
        }

        *p = 0;  // zero indicates the end of targets.

        return p - targets;
    }

    const header_type *header() const
    {
        return header_;
    }

    const state_type *states() const
    {
        return states_;
    }

    bool owner() const
    {
        return owner_;
    }

  protected:
    explicit basic_trie(void *header, void *states);
    size_type find_base(const char_type *inputs,
                        const extremum_type &extremum);
    size_type relocate(size_type stand,
                       size_type s,
                       const char_type *inputs,
                       const extremum_type &extremum);

    bool check_transition(size_type s, size_type t) const
    {
        return (t > 0 && t < header_->size && check(t) == s)?true:false;
    }

    char_type char_in(const char ch) const
    {
        return static_cast<unsigned char>(ch + 1);
    }

    char char_out(char_type ch) const
    {
        return static_cast<char>(ch - 1);
    }

    void inflate(size_type size)
    {
        // align with 4k
        size_type nsize = (((header_->size + size) >> 12) + 1) << 12;
        states_ = static_cast<state_type *>(realloc(states_,
                                                  nsize * sizeof(state_type)));
        if (!states_)
            throw std::bad_alloc();
        memset(states_ + header_->size,
               0,
               (nsize - header_->size) * sizeof(state_type));
        header_->size = nsize;
    }

  private:
    header_type *header_;
    state_type *states_;
    size_type last_base_;
    bool owner_;
};
#endif  // TRIE_H_

// vim: ts=4 sw=4 ai et
