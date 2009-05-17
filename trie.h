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
        char magic[36];
        size_type size;
    } header_type;

    typedef struct {
        char_type max;
        char_type min;
    } extremum_type;


    explicit basic_trie(size_type size = 1024);
    ~basic_trie();

    void insert(const char *inputs, size_t length, value_type val);
    value_type search(const char *inputs, size_t length);
    void trace(size_type s);

  protected:

    static const char_type kCharsetSize = 257;
    static const char_type kTerminator = kCharsetSize;

    header_type *header_;
    state_type *states_;
    unsigned char *mmap_;
    size_type last_base_;
    std::vector<size_type> trace_stack_;

    char_type char_in(const char ch)
    {
        return static_cast<unsigned char>(ch + 1);
    }

    char char_out(char_type ch)
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

    value_type base(size_type s) { return states_[s].base; }
    value_type check(size_type s) { return states_[s].check; }
    void set_base(size_type s, value_type val)
    {
        states_[s].base = val;
    }
    void set_check(size_type s, value_type val)
    {
        states_[s].check = val;
    }

    bool check_transition(size_type s, size_type t)
    {
        return (t > 0 && t < header_->size && check(t) == s)?true:false;
    }

    // Get next state from s with input ch
    size_type next(size_type s, char_type ch)
    {
        if (s + ch >= header_->size)
            inflate(ch);
        return base(s) + ch;
    }

    // Get prev state from s with input ch
    size_type prev(size_type s)
    {
        return check(s);
    }

    // Go forward from state s with inputs, returns the last 
    // states and the mismatch position by pointer
    size_type go_forward(size_type s,
                         const char *inputs, 
                         size_t length, 
                         const char **mismatch)
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
public:
    // Go backward from state s with inputs, returns the last
    // states and the mismatch position by pointer
    size_type go_backward(size_type s,
                          const char *inputs,
                          size_t length,
                          const char **mismatch)
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
protected:
    // Find out all exists targets from s and store them into *targets.
    // If max is not null, the maximum char makes state transit from s to
    // those targets will be stored into max. Same to min.
    size_type find_exist_target(size_type s,
                                char_type *targets,
                                extremum_type *extremum)
    {
        char_type ch;
        char_type *p;

        for (ch = 1, p = targets; ch < kCharsetSize + 1; ch++) {
            if (check_transition(s, next(s, ch))) {
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

    size_type find_base(const char_type *inputs,
                        const extremum_type &extremum);
    size_type relocate(size_type stand,
                       size_type s,
                       const char_type *inputs,
                       const extremum_type &extremum);
    size_type create_link(size_type s, char_type ch);
};
#endif  // TRIE_H_

// vim: ts=4 sw=4 ai et
