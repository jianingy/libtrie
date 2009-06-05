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
#include <string>
#include <map>
#include <set>

template<typename T> class trie_relocator_interface {
  public:
    virtual void relocate(T s, T t) = 0;
};

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

    explicit basic_trie(size_type size = 1024,
                        trie_relocator_interface<size_type> *relocator = NULL);
    basic_trie(const basic_trie &trie);
    void clone(const basic_trie &trie);
    basic_trie &operator=(const basic_trie &trie);
    ~basic_trie();
    void insert(const char *inputs, size_t length, value_type val);
    value_type search(const char *inputs, size_t length) const;
    void trace(size_type s) const;
    size_type create_transition(size_type s, char_type ch);

    static const basic_trie *create_from_memory(void *header,
                                                void *states)
    {
        return new basic_trie(header, states);
    }

    void set_relocator(trie_relocator_interface<size_type> *relocator)
    {
        relocator_ = relocator;
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

    size_type go_forward_reverse(size_type s,
                                 const char *inputs,
                                 size_t length,
                                 const char **mismatch) const
    {
        const char *p;
        for (p = inputs + length - 1; p >= inputs; p--) {
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
        for (p = inputs; p < inputs + length; p++) {
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

    size_type go_backward_reverse(size_type s,
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

    bool check_transition(size_type s, size_type t) const
    {
        return (s > 0 && t > 0 && t < header_->size && check(t) == s)?true:false;
    }
    bool check_reverse_transition(size_type s, char_type ch) const
    {
        return ((next(prev(s), ch) == s) 
                && check_transition(prev(s), next(prev(s), ch)));
    }

    static char_type char_in(const char ch)
    {
        return static_cast<unsigned char>(ch + 1);
    }

    static char char_out(char_type ch)
    {
        return static_cast<char>(ch - 1);
    }

  protected:
    explicit basic_trie(void *header, void *states);
    size_type find_base(const char_type *inputs,
                        const extremum_type &extremum);
    size_type relocate(size_type stand,
                       size_type s,
                       const char_type *inputs,
                       const extremum_type &extremum);

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
  private:
    header_type *header_;
    state_type *states_;
    size_type last_base_;
    bool owner_;
    trie_relocator_interface<size_type> *relocator_;
};

template<typename T>
class trie_relocator: public trie_relocator_interface<basic_trie::size_type> {
  public:
    typedef basic_trie::size_type size_type;
    typedef void (T::*relocate_function)(size_type, size_type);
    trie_relocator(T *who, relocate_function relocate)
        :who_(who), relocate_(relocate)
    {
    }

    void relocate(size_type s, size_type t)
    {
        (who_->*relocate_)(s, t);
    }
  private:
    T *who_;
    relocate_function relocate_;

    trie_relocator(const trie_relocator &);
    void operator=(const trie_relocator &);
};

class trie {
  public:
    typedef basic_trie::size_type size_type;
    trie();
    ~trie();
    void insert(const char *inputs, size_t length);
    int search(const char *inputs, size_t length);
    const basic_trie *front_trie() const
    {
        return lhs_;
    }
    const basic_trie *rear_trie() const
    {
        return rhs_;
    }

//  protected:
    size_type rhs_append(const char *inputs, size_t length);
    void lhs_insert(size_type s, const char *inputs, size_t length);
    void rhs_clean_more(size_type t);
    void rhs_insert(size_type s, size_type r, 
                    const char *match, size_t match_length,
                    const char *remain, size_t remain_length,
                    char ch, bool terminator);

    void remove_accept_state(size_type s)
    {
        assert(s > 0);
        rhs_->set_base(s, 0);
        rhs_->set_check(s, 0);
        if (count_referer(s) == 0)
            accept_[refer_[s].accept_index].accept = 0;
        refer_.erase(s);
    }

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
        size_type i;

        if (refer_.find(t) != refer_.end()) {
            i = find_index_entry(s);
            index_[i].index = refer_[t].accept_index;
        } else {
            i = find_accept_entry(find_index_entry(s));
            accept_[i].accept = t;
            refer_[t].accept_index = i;
        }
        assert(lhs_->base(s) < 0);
        refer_[t].referer.insert(s);
    }

    size_t count_referer(size_type s) const
    {
        std::map<size_type, refer_type>::const_iterator found(refer_.find(s));
        if (found == refer_.end())
            return 0;
        else 
            return found->second.referer.size();
    }

    size_type find_index_entry(size_type s)
    {
        assert(lhs_->base(s) <= 0);
        if (lhs_->base(s) >= 0) {
            if (next_index_ >= index_size_) {
                size_type nsize = next_index_ * 2;
                //size_type nsize = (((index_size_ + next) >> 12) + 1) << 12;
                index_ = static_cast<index_type *>
                        (realloc(index_, nsize * sizeof(index_type)));
                if (!index_)
                    throw std::bad_alloc();
                memset(index_ + index_size_, 0,
                       (nsize - index_size_) * sizeof(index_type));
                index_size_ = nsize;
            }
            //printf("index s = %d\n", s);
            lhs_->set_base(s, -next_index_);
            ++next_index_;
        }
        return -lhs_->base(s);
    }

    size_type find_accept_entry(size_type i)
    {
        if (!index_[i].index) {
            if (next_accept_ >= accept_size_) {
                size_type nsize = next_accept_ * 2;
                accept_ = static_cast<accept_type *>
                        (realloc(accept_, nsize * sizeof(accept_type)));
                if (!accept_)
                    throw std::bad_alloc();
                memset(accept_ + accept_size_, 0,
                       (nsize - accept_size_) * sizeof(accept_type));
                accept_size_ = nsize;
            }
            //printf("accept i = %d\n", i);
            index_[i].index = next_accept_;
            ++next_accept_;
        }
        return index_[i].index;
    }

    size_t outdegree(size_type s) const
    {
        basic_trie::char_type ch;
        size_t degree = 0;
        for (ch = 1; ch < basic_trie::kCharsetSize + 1; ch++) {
            size_type t = rhs_->next(s, ch);
            if (t >= rhs_->header()->size)
                break;
            if (rhs_->check_transition(s, t))
                degree++;
        }

        return degree;
    }

    bool rhs_clean_one(size_type t)
    {
        size_type s = rhs_->prev(t);
        if (s > 0
            && t == rhs_->next(s, basic_trie::kTerminator)
            && count_referer(t) == 0) {
            // delete one
            remove_accept_state(t);
            return true;
        }
        return false;
    }

    void trace_table(size_type istart,
                     size_type astart) const
    {
        static const size_type dsize = 40;
        size_type i;
        printf("========================================");
        printf("\nSEQ     |");
        for (i = istart; i < dsize && i < index_size_; i++)
            printf("%4d ", i);
        printf("\nDATA    |");
        for (i = istart; i < dsize && i < index_size_; i++)
            printf("%4d ", index_[i].data);
        printf("\nINDEX   |");
        for (i = istart; i < dsize && i < index_size_; i++)
            printf("%4d ", index_[i].index);
        printf("\nCOUNT   |");
        for (i = astart; i < dsize && i < accept_size_; i++)
            printf("%4d ", count_referer(accept_[i].accept));
        printf("\nACCEPT  |");
        for (i = astart; i < dsize && i < accept_size_; i++)
            printf("%4d ", accept_[i].accept);
        printf("\n========================================\n");
        std::set<size_type>::const_iterator it;       
        std::map<size_type, refer_type>::const_iterator mit;
        for (mit = refer_.begin(); mit != refer_.end(); mit++) {
            printf("%4d: ", mit->first);
            for (it = mit->second.referer.begin();
                 it != mit->second.referer.end();
                 it++)
                printf("%4d ", *it);
            printf("\n");
        }
        printf("========================================\n");
    }

    void relocate_front(size_type s, size_type t)
    {
        if (lhs_->base(s) < 0) {
            size_type r = link_state(s);
            if (refer_.find(r) != refer_.end()) {
                refer_[r].referer.erase(s);
                assert(lhs_->base(t) < 0);
                refer_[r].referer.insert(t);
            }
        }
    }    

    void relocate_rear(size_type s, size_type t)
    {
        if (refer_.find(s) != refer_.end()) {
            accept_[refer_[s].accept_index].accept = t;
            refer_[t] = refer_[s];
            refer_.erase(s);
        } else if (stand_ == s) {
            stand_ = t;
        }
    }    

  private:
    typedef struct {
        basic_trie::value_type data;
        size_type index;
    } index_type;
    typedef struct {
        size_type accept;
    } accept_type;
    typedef struct {
        size_type accept_index;
        std::set<size_type> referer;
    } refer_type;

    basic_trie *lhs_, *rhs_;
    index_type *index_;
    accept_type *accept_;
    size_type index_size_, accept_size_;
    std::map<size_type, refer_type> refer_;
    std::string exists_;
    size_type next_accept_, next_index_;
    trie_relocator<trie> *front_relocator_, *rear_relocator_;
    size_type stand_;
};

#endif  // TRIE_H_

// vim: ts=4 sw=4 ai et
