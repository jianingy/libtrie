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
#include <map>

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

class trie {
  public:
    typedef basic_trie::size_type size_type;
    trie();
    ~trie();
//  protected:
    bool check_separator(size_type s) const
    {
        return (lhs_->base(s) < 0)?true:false;
    }
    
    size_type link_state(size_type s) const
    {
        return accept_[index_[-lhs_->base(s)].index].accept;
    }

    void set_link(size_type s, size_type t)
    {
        if (backref_[t]) {
            index_[index_entry(s)].index = backref_[t];
        } else {
            size_type i = accept_entry(index_entry(s));
            accept_[i].accept = t;
            accept_[i].count++;
            backref_[t] = i;
        }
    }
    
    size_type index_entry(size_type s)
    {
        static size_type next = 1;
        if (!lhs_->base(s)) {
            if (next >= index_size_) {
                size_type nsize = next * 2 + 1;
                index_ = static_cast<index_type *>
                        (realloc(index_, nsize * sizeof(index_type)));
                if (!index_)
                    throw std::bad_alloc();
                memset(index_ + index_size_, 0,
                       (nsize - index_size_) * sizeof(index_type));
                index_size_ = nsize; 
            }
            lhs_->set_base(s, -next);
            ++next;
        }
        return -lhs_->base(s);
    }

    size_type accept_entry(size_type i)
    {
        static size_type next = 1;
        if (!index_[i].index) {
            if (next >= accept_size_) {
                size_type nsize = next * 2 + 1;
                accept_ = static_cast<accept_type *>
                        (realloc(accept_, nsize * sizeof(accept_type)));
                if (!accept_)
                    throw std::bad_alloc();
                memset(accept_ + accept_size_, 0,
                       (nsize - accept_size_) * sizeof(accept_type));
                accept_size_ = nsize; 
            }
            index_[i].index = next++;
        }
        return index_[i].index;
    }

    void trace_table(size_type istart,
                     size_type astart) const
    {
        static const size_type dsize = 10;
        size_type i;
        printf("========================================");
        printf("\nSEQ     |");
        for (i = istart; i < dsize && i < index_size_; i++)
            printf("%2d ", i);
        printf("\nDATA    |");
        for (i = istart; i < dsize && i < index_size_; i++)
            printf("%2d ", index_[i].data);
        printf("\nINDEX   |");
        for (i = istart; i < dsize && i < index_size_; i++)
            printf("%2d ", index_[i].index);
        printf("\nCOUNT   |");
        for (i = astart; i < dsize && i < accept_size_; i++)
            printf("%2d ", accept_[i].count);
        printf("\nACCEPT  |");
        for (i = astart; i < dsize && i < accept_size_; i++)
            printf("%2d ", accept_[i].accept);
        printf("\n========================================\n");
    }

  private:
    typedef struct {
        basic_trie::value_type data;
        size_type index;
    } index_type;
    typedef struct {
        size_type count;
        size_type accept;
    } accept_type;

    basic_trie *lhs_, *rhs_;
    index_type *index_;
    accept_type *accept_;
    size_type index_size_, accept_size_;
    std::map<size_type, size_type> backref_;
};
#endif  // TRIE_H_

// vim: ts=4 sw=4 ai et
