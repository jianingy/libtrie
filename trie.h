// Copyright Jianing Yang <jianingy.yang@gmail.com> 2009

#ifndef TRIE_H_
#define TRIE_H_

/*
 * Reference:
 * [1] J.AOE An Efficient Digital Search Algorithm by
 *           Using a Double-Array Structure
 * [2] J.AOE A Trie Compaction Algorithm for a Large Set of Keys
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
#include <stdio.h>

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cassert>
#include <string>
#include <map>
#include <set>
#include <deque>

class trie_interface {
  public:
    typedef int32_t value_type;
    typedef int32_t size_type;
    typedef int32_t char_type;

    trie_interface() {}
    explicit trie_interface(const char *filename) {}
    virtual void insert(const char *inputs, size_t length,
                        value_type value) = 0;
    virtual bool search(const char *inputs, size_t length,
                        value_type *value) const = 0;
    virtual void build(const char *filename, bool verbose = false) = 0;
    virtual ~trie_interface() = 0;
};

template<typename T> class trie_relocator_interface {
  public:
    virtual void relocate(T s, T t) = 0;
    virtual ~trie_relocator_interface() {}
};

template<typename T>
T* resize(T *ptr, size_t old_size, size_t new_size)
{
    T *new_block = new T[new_size];
    if (ptr) {
        std::copy(ptr, ptr + old_size, new_block);
        delete []ptr;
        memset(new_block + old_size, 0, (new_size - old_size) * sizeof(T));
        return new_block;
    } else {
        memset(new_block, 0, new_size * sizeof(T));
        return new_block;
    }
}

class trie_input_converter
{
  public:
    typedef trie_interface::size_type size_type;
    typedef trie_interface::char_type char_type;

    static const char_type kCharsetSize = 257;
    static const char_type kTerminator = kCharsetSize;

    explicit trie_input_converter(size_t length = 8)
        :inside_(NULL), inside_size_(0)
    {
        if (length)
            resize_inside(length);
    }

    ~trie_input_converter()
    {
        delete []inside_;
        inside_ = NULL;
        inside_size_ = 0;
    }

    static char_type char_in(const char ch)
    {
        return static_cast<unsigned char>(ch + 1);
    }

    static char char_out(char_type ch)
    {
        return static_cast<char>(ch - 1);
    }

    const char_type *convert(const char *inputs,
                             size_t length)
    {
        size_t i;

        if (length > inside_size_)
            resize_inside(length + 1);
        for (i = 0; i < length; i++)
            inside_[i] = char_in(inputs[i]); 
        inside_[i] = kTerminator;

        return inside_;
    }

  protected:
    void resize_inside(size_type size)
    {
        // align with 4k
        size_type nsize = (((inside_size_ + size) >> 12) + 1) << 12;
        inside_ = resize<char_type>(inside_, inside_size_, nsize);
        inside_size_ = nsize;
    }
  private:
    char_type *inside_;
    size_t inside_size_;
};

class basic_trie
{
  public:
    typedef trie_interface::value_type value_type;
    typedef trie_interface::size_type size_type;
    typedef trie_interface::char_type char_type;

    static const char_type kCharsetSize = trie_input_converter::kCharsetSize;
    static const char_type kTerminator = trie_input_converter::kTerminator;

    typedef struct {
        size_type base;
        size_type check;
    } state_type;

    // this struct should be 64 bytes long for compatible
    typedef struct {
        size_type size;
        char unused[60];
    } header_type;

    typedef struct {
        char_type max;
        char_type min;
    } extremum_type;

    explicit basic_trie(size_type size = 1024,
                        trie_relocator_interface<size_type> *relocator = NULL);
    explicit basic_trie(void *header, void *states);
    basic_trie(const basic_trie &trie);
    void clone(const basic_trie &trie);
    basic_trie &operator=(const basic_trie &trie);
    ~basic_trie();
    void insert(const char *key, size_t length, value_type value);
    bool search(const char *key, size_t length, value_type *value) const;
    size_type create_transition(size_type s, char_type ch);
    size_type find_base(const char_type *inputs,
                        const extremum_type &extremum);

    void trace(size_type s) const;

    static const basic_trie *create_from_memory(void *header,
                                                void *states)
    {
        return new basic_trie(header, states);
    }

    void set_relocator(trie_relocator_interface<size_type> *relocator)
    {
        relocator_ = relocator;
    }

    size_type base(size_type s) const
    {
        return states_[s].base;
    }

    size_type check(size_type s) const
    {
        return states_[s].check;
    }

    void set_base(size_type s, size_type val)
    {
        states_[s].base = val;
    }

    void set_check(size_type s, size_type val)
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
                         const char_type *inputs,
                         const char_type **mismatch) const
    {
        assert(mismatch);
        const char_type *p = inputs;
        do {
            size_type t = next(s, *p);
            if (!check_transition(s, t)) {
                *mismatch = p;
                return s;
            }
            s = t;
        } while (*p++ != kTerminator);
        *mismatch = NULL;
        return s;
    }

    size_type go_forward_reverse(size_type s,
                                 const char_type *inputs,
                                 const char_type **mismatch) const
    {
        assert(mismatch);
        const char_type *p = inputs;
        while (*p != kTerminator)
            p++;
        do {
            size_type t = next(s, *p);
            if (!check_transition(s, t)) {
                *mismatch = p;
                return s;
            }
            s = t;
        } while (p-- > inputs);
        *mismatch = NULL;
        return s;
    }

    // Go backward from state s with inputs, returns the last
    // states and the mismatch position by pointer
    size_type go_backward(size_type s,
                          const char_type *inputs,
                          const char_type **mismatch) const
    {
        assert(mismatch);
        const char_type *p = inputs;
        do {
            size_type t = prev(s);
            if (!check_transition(t, next(t, *p))) {
                *mismatch = p;
                return s;
            }
            s = t;
        } while (*p++ != kTerminator);
        *mismatch = NULL;
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
        return (s > 0
                && t > 0
                && t < header_->size && check(t) == s)?true:false;
    }
    bool check_reverse_transition(size_type s, char_type ch) const
    {
        return ((next(prev(s), ch) == s)
                && check_transition(prev(s), next(prev(s), ch)));
    }

  protected:
    size_type relocate(size_type stand,
                       size_type s,
                       const char_type *inputs,
                       const extremum_type &extremum);

    void resize_state(size_type size)
    {
        // align with 4k
        size_type nsize = (((header_->size + size) >> 12) + 1) << 12;
        states_ = resize<state_type>(states_, header_->size, nsize);
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

        for (ch = 1, p = targets;
            ch < trie_input_converter::kCharsetSize + 1;
            ch++) {
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
    mutable trie_input_converter converter_;
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

class double_trie: public trie_interface {
  public:
    typedef basic_trie::size_type size_type;
    typedef basic_trie::value_type value_type;

    // this struct should be 64 bytes long for compatible
    typedef struct {
        char magic[16];
        size_type index_size;
        size_type accept_size;
        char unused[40];
    } header_type;

    double_trie();
    explicit double_trie(const char *filename);
    ~double_trie();
    void insert(const char *key, size_t length, value_type value);
    bool search(const char *key, size_t length, value_type *value) const;
    void build(const char *filename, bool verbose = false);
    const basic_trie *front_trie() const
    {
        return lhs_;
    }

    const basic_trie *rear_trie() const
    {
        return rhs_;
    }

    void trace_table(size_type istart,
                     size_type astart) const
    {
        static const size_type dsize = 20;
        size_type i;
        fprintf(stderr,"========================================");
        fprintf(stderr,"\nSEQ     |");
        for (i = istart; i < dsize && i < header_->index_size; i++)
            fprintf(stderr,"%4d ", i);
        fprintf(stderr,"\nDATA    |");
        for (i = istart; i < dsize && i < header_->index_size; i++)
            fprintf(stderr,"%4d ", index_[i].data);
        fprintf(stderr,"\nINDEX   |");
        for (i = istart; i < dsize && i < header_->index_size; i++)
            fprintf(stderr,"%4d ", index_[i].index);
        fprintf(stderr,"\nCOUNT   |");
        for (i = astart; i < dsize && i < header_->accept_size; i++)
            fprintf(stderr,"%4d ", count_referer(accept_[i].accept));
        fprintf(stderr,"\nACCEPT  |");
        for (i = astart; i < dsize && i < header_->accept_size; i++)
            fprintf(stderr,"%4d ", accept_[i].accept);
        fprintf(stderr,"\n========================================\n");
        std::set<size_type>::const_iterator it;
        std::map<size_type, refer_type>::const_iterator mit;
        for (mit = refer_.begin(); mit != refer_.end(); mit++) {
            fprintf(stderr,"%4d: ", mit->first);
            for (it = mit->second.referer.begin();
                 it != mit->second.referer.end();
                 it++)
                fprintf(stderr,"%4d ", *it);
            fprintf(stderr,"\n");
        }
        fprintf(stderr,"========================================\n");
    }

  protected:
    size_type rhs_append(const char_type *inputs);
    void lhs_insert(size_type s, const char_type *inputs, value_type value);
    void rhs_clean_more(size_type t);
    void rhs_insert(size_type s, size_type r, 
                    const std::vector<char_type> &match,
                    const char_type *remain, char_type ch, size_type value);

    void remove_accept_state(size_type s)
    {
        assert(s > 0);
        rhs_->set_base(s, 0);
        rhs_->set_check(s, 0);
        free_accept_entry(s);
    }

    bool check_separator(size_type s) const
    {
        return (lhs_->base(s) < 0)?true:false;
    }

    size_type link_state(size_type s) const
    {
        return accept_[index_[-lhs_->base(s)].index].accept;
    }

    size_type set_link(size_type s, size_type t)
    {
        size_type i;

        if (refer_.find(t) != refer_.end()) {
            i = find_index_entry(s);
            index_[i].index = refer_[t].accept_index;
        } else {
            i = find_index_entry(s);
            size_type acc = find_accept_entry(i);
            accept_[acc].accept = t;
            refer_[t].accept_index = acc;
        }
        assert(lhs_->base(s) < 0);
        refer_[t].referer.insert(s);

        return i;
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
        size_type next;

        if (lhs_->base(s) >= 0) {
            if (free_index_.size() > 0) {
                next = free_index_.front();
                free_index_.pop_front();
            } else {
                next = next_index_;
                ++next_index_;
            }
            if (next >= header_->index_size) {
                size_type nsize = (((next * 2) >> 12) + 1) << 12;
                index_ = resize<index_type>(index_, header_->index_size, nsize);
                assert(index_[next].index == 0);
                header_->index_size = nsize;
            }
            lhs_->set_base(s, -next);
        }
        return -lhs_->base(s);
    }

    size_type find_accept_entry(size_type i)
    {
        size_type next;

        if (!index_[i].index) {
            if (free_accept_.size() > 0) {
                next = free_accept_.front();
                free_accept_.pop_front();
            } else {
                next = next_accept_;
                ++next_accept_;
            }
            if (next >= header_->accept_size) {
                size_type nsize = (((next * 2) >> 12) + 1) << 12;
                accept_ = resize<accept_type>(accept_,
                                              header_->accept_size,
                                              nsize);
                header_->accept_size = nsize;
            }
            index_[i].index = next;
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
            free_accept_entry(s);
        } else if (stand_ == s) {
            stand_ = t;
        }
    }

    void free_accept_entry(size_type s)
    {
        if (refer_.find(s) != refer_.end()) {
            // XXX: check what cause the inequivalent
            if (s > 0
                && count_referer(s) == 0
                && refer_[s].accept_index < header_->accept_size
                && refer_[s].accept_index > 0) {
                accept_[refer_[s].accept_index].accept = 0;
                free_accept_.push_back(refer_[s].accept_index);
            }
            refer_.erase(s);
        }
    }

  private:
    typedef struct {
        value_type data;
        size_type index;
    } index_type;
    typedef struct {
        size_type accept;
    } accept_type;
    typedef struct {
        size_type accept_index;
        std::set<size_type> referer;
    } refer_type;

    header_type *header_;
    basic_trie *lhs_, *rhs_;
    index_type *index_;
    accept_type *accept_;
    std::map<size_type, refer_type> refer_;
    std::vector<char_type> exists_;
    size_type next_accept_, next_index_;
    trie_relocator<double_trie> *front_relocator_, *rear_relocator_;
    size_type stand_;
    std::deque<size_type> free_accept_;
    std::deque<size_type> free_index_;
    void *mmap_;
    size_t mmap_size_;
    static const char magic_[16];
    mutable trie_input_converter converter_;
};

class single_trie: public trie_interface
{
  public:
    typedef basic_trie::char_type char_type;
    typedef basic_trie::size_type size_type;
    typedef basic_trie::value_type value_type;
    typedef size_type suffix_type;
    typedef struct {
        char magic[16];
        size_type suffix_size;
        char unused[44];
    } header_type;

    typedef struct {
        char_type *data;
        size_t size;
    } common_type;

    single_trie();
    explicit single_trie(const char *filename);
    ~single_trie();
    void insert(const char *key, size_t length, value_type value);
    bool search(const char *key, size_t length, value_type *value) const;
    void build(const char *filename, bool verbose);

    const basic_trie *trie()
    {
        return trie_;
    }

    const suffix_type *suffix()
    {
        return suffix_;
    }

    void trace_suffix(size_type start, size_type count)
    {
        size_type i;
        for (i = start; i < header_->suffix_size && i < count; i++) {
            if (suffix_[i] == basic_trie::kTerminator)
                printf("[%d:#]", i);
            else if (isgraph(trie_input_converter::char_out(suffix_[i])))
                printf("[%d:%c]", i, trie_input_converter::char_out(suffix_[i]));
            else
                printf("[%d:%x]", i, suffix_[i]);
        }
        printf("\n");
    }

  protected:
    void resize_suffix(size_type size)
    {
        // align with 4k
        size_type nsize = (((header_->suffix_size + size) >> 12) + 1) << 12;
        suffix_ = resize<suffix_type>(suffix_, header_->suffix_size, nsize);
        header_->suffix_size = nsize;
    }

    void resize_common(size_type size)
    {
        // align with 4k
        size_type nsize = (((common_.size + size) >> 12) + 1) << 12;
        common_.data = resize<char_type>(common_.data, common_.size, nsize);
        common_.size = nsize;
    }

    void insert_suffix(size_type s, const char_type *inputs, value_type value);
    void create_branch(size_type s, const char_type *inputs, value_type value);

  private:
    basic_trie *trie_;
    suffix_type *suffix_;
    header_type *header_;
    size_type next_suffix_;
    common_type common_;
    void *mmap_;
    size_t mmap_size_;
    static const char magic_[16];
    mutable trie_input_converter converter_;
};
#endif  // TRIE_H_

// vim: ts=4 sw=4 ai et
