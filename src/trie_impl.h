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
#ifndef TRIE_IMPL_H_
#define TRIE_IMPL_H_

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
#include <stdint.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cassert>
#include <string>
#include <map>
#include <set>
#include <deque>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "trie.h"

BEGIN_TRIE_NAMESPACE

/**
 * Represents an interface to trie state relocator.
 *
 * A trie state relocator will be called when a state is being moved
 * by relocate method of basic_trie during create_transition.
 *
 * @param T Type of state index
 */
template<typename T> class trie_relocator_interface {
  public:

    /**
     * Notifies state changing
     *
     * @param s The original state index
     * @param t The target state index to be moved to.
     */
    virtual void relocate(T s, T t) = 0;

    /**
     * Destructs a trie_relocator
     */
    virtual ~trie_relocator_interface() {}

    // XXX: Disallow copy constructor and operator =
};

/**
 * Resizes a buffer.
 * This function works like realloc(3). If ptr is NULL, it will allocate
 * a new buffer. If new_size is zero, the function acts like a free(3). In
 * addition, this function sets all newly allocated units to zero.
 *
 * @param ptr Pointer to the buffer.
 * @param old_size Original size of the buffer.
 * @param new_size Expected size of the buffer.
 * @return Pointer to new buffer with expected size.
 */
template<typename T>
T* resize(T *ptr, size_t old_size, size_t new_size)
{
    T *new_block = reinterpret_cast<T *>(realloc(ptr, new_size * sizeof(T)));
    if (new_size && ptr) {
        memset(new_block + old_size, 0, (new_size - old_size) * sizeof(T));
    } else if (new_size) {
        memset(new_block, 0, new_size * sizeof(T));
    }
    return new_block;
#if 0
    if (ptr) {
        if (!new_size) {
            delete []ptr;
            return NULL;
        } else {
            T *new_block = new T[new_size];
            std::copy(ptr, ptr + old_size, new_block);
            delete []ptr;
            memset(new_block + old_size, 0, (new_size - old_size) * sizeof(T));
            return new_block;
        }
    } else {
        T *new_block = new T[new_size];
        memset(new_block, 0, new_size * sizeof(T));
        return new_block;
    }
#endif
}

/// Represents a double-array with basic operations.
class basic_trie
{
  public:
    /// Default initial size of states buffer.
    static const size_t kDefaultStateSize = 4096;

    /// Represents a state
    typedef struct {
        size_type base;  ///< The BASE value according to J.Aoe's paper
        size_type check; ///< The CHECK value according to J.Aoe's paper
    } state_type;

    /**
     * Represents some information about basic_trie.
     */
    typedef struct {
        size_type size;  ///< Size of state buffer
        char unused[60]; ///< Unused, for 32/64 bits compatible.
    } header_type;

    /**
     * Represents a pair of extremum. It is used to improve the
     * performance of find_base method.
     */
    typedef struct {
        char_type max; ///< A Maximum value
        char_type min; ///< A Minimum value
    } extremum_type;

    /**
     * Constructs an empty basic_trie.
     *
     * @param size Initial size of state buffer.
     * @param relocator A trie relocator if needed, @see
     *                  trie_relocator_interface.
     */
    explicit basic_trie(size_type size = kDefaultStateSize,
                        trie_relocator_interface<size_type> *relocator = NULL);

    /**
     * Constructs a basic_trie from given pointers.
     *
     * @param header Pointer to an existing header data.
     * @param states Pointer to an existing state data buffer.
     */
    explicit basic_trie(void *header, void *states);

    /**
     * Constructs a copy from trie.
     *
     * @param trie Const reference to a basic_trie.
     */
    basic_trie(const basic_trie &trie);

    /**
     * Copies from a trie.
     *
     * @param trie Const reference to a basic_trie.
     */
    basic_trie &operator=(const basic_trie &trie);

    /**
     * Copies from a trie. see also copy constructor and operator =.
     *
     * @param trie Const reference to a basic_trie.
     */
    void clone(const basic_trie &trie);

    /// Destructs a basic_trie.
    ~basic_trie();

    /// Stores a value by given key.
    void insert(const key_type &key, const value_type &value);

    /// Retrieves a value by given key.
    bool search(const key_type &key, value_type *value) const;

    /**
     * Retrieves all key-value pairs match given prefix.
     *
     * @param prefix The prefix.
     * @param[out] result Result set.
     */
    size_t prefix_search(const key_type &prefix, result_type *result) const;

    /**
     * Retrieves all key-value pairs match given prefix from state s.
     *
     * @param s Start state.
     * @param p Mismatch character buffer.
     * @param[out] store Temporary storage for found keys.
     * @param[out] result Result set.
     * @return The number of elements in the result set.
     */
    size_t prefix_search_aux(size_type s,
                             const char_type *p,
                             key_type *store,
                             result_type *result) const;

    /**
     * Creates a new tranisition from state s with input char_type.
     *
     * @param s Start state.
     * @param ch The input char_type.
     */
    size_type create_transition(size_type s, char_type ch);

    /**
     * Finds a free BASE value for storing all inputs.
     *
     * @param inputs Inputs to be stored.
     * @param extremum The max and min value in inputs.
     * @return The proper BASE value.
     */
    size_type find_base(const char_type *inputs,
                        const extremum_type &extremum);

    /**
     * Prints all outcome transition from state s.
     *
     * @param s Start state.
     */
    void trace(size_type s) const;

    /**
     * Returns a pointer to a newly created basic_trie.
     *
     * @param header Pointer to an existing header data.
     * @param states Pointer to an existing state data buffer.
     * @return Pointer to the newly created basic_trie.
     */
    static const basic_trie *create_from_memory(void *header,
                                                void *states)
    {
        return new basic_trie(header, states);
    }

    /**
     * Sets the last available BASE value. It uses to improve the
     * performance of find_base.
     *
     * @param base The BASE value.
     */
    void set_last_base(size_type base)
    {
        last_base_ = base;
    }

    /**
     * Sets a new state relocator.
     *
     * @param relocator The relocator to be used.
     */
    void set_relocator(trie_relocator_interface<size_type> *relocator)
    {
        relocator_ = relocator;
    }

    /// Get the BASE value of state s.
    size_type base(size_type s) const
    {
        return states_[s].base;
    }

    /// Get the CHECK value of state s.
    size_type check(size_type s) const
    {
        return states_[s].check;
    }

    /// Set a new BASE value of state s.
    void set_base(size_type s, size_type val)
    {
        states_[s].base = val;
        if (s > max_state_)
            max_state_ = s;
    }

    /// Set a new CHECK value of state s.
    void set_check(size_type s, size_type val)
    {
        states_[s].check = val;
    }

    /// Gets next state from s with input ch.
    size_type next(size_type s, char_type ch) const
    {
        return base(s) + ch;
    }

    /// Get previous state from s with input ch.
    size_type prev(size_type s) const
    {
        return check(s);
    }

    /**
     * Goes forward from state s with inputs. Returns the last arrived
     * state and sets mismatch to mismatch position.
     */
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
        } while (*p++ != key_type::kTerminator);
        *mismatch = NULL;
        return s;
    }

    /**
     * Goes forward from state s with reverse inputs. Returns the last
     * arrived state and sets mismatch to mismatch position.
     */
    size_type go_forward_reverse(size_type s,
                                 const char_type *inputs,
                                 const char_type **mismatch) const
    {
        assert(mismatch);
        const char_type *p = inputs;
        while (*p != key_type::kTerminator)
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

    /**
     * Goes backward from state s with inputs. Returns the last arrived
     * state and sets mismatch to mismatch position.
     */
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
        } while (*p++ != key_type::kTerminator);
        *mismatch = NULL;
        return s;
    }

    /**
     * Returns a pointer to a basic_trie header whose size is
     * exactly the number of used items in state buffer.
     */
    const header_type *compact_header() const
    {
        memcpy(&compact_header_, header_, sizeof(header_type));
        compact_header_.size = max_state_ + 1;
        return &compact_header_;
    }

    /// Returns a pointer to header.
    const header_type *header() const
    {
        return header_;
    }

    /// Returns a pointer to state buffer.
    const state_type *states() const
    {
        return states_;
    }

    /// Returns the number of elements used in state buffer.
    size_type max_state() const
    {
        return max_state_;
    }

    /// Returns true if a basic_trie owns the memory of its data.
    bool owner() const
    {
        return owner_;
    }

    /// Returns true if there is a transition from s to t.
    bool check_transition(size_type s, size_type t) const
    {
        return (s > 0
                && t > 0
                && t < header_->size && check(t) == s)?true:false;
    }

    /// Returns true if s can be traced back by input ch.
    bool check_reverse_transition(size_type s, char_type ch) const
    {
        return ((next(prev(s), ch) == s)
                && check_transition(prev(s), next(prev(s), ch)));
    }

  protected:
    /**
     * Relocates all target states linked from state s by changing the BASE
     * of s.
     *
     * @param stand A state which is using while relocating
     * @param s Start state
     * @param inputs More char_types to be fitted by relocating.
     * @param extremum The max and min value of inputs
     * @return New base.
     */
    size_type relocate(size_type stand,
                       size_type s,
                       const char_type *inputs,
                       const extremum_type &extremum);

    /// Resizes state buffer.
    void resize_state(size_type size)
    {
        // align with 4k
        size_type nsize = (((header_->size * 2 + size) >> 12) + 1) << 12;
        states_ = resize(states_, header_->size, nsize);
        header_->size = nsize;
    }

    /**
     * Finds out all exists targets from s and stores them into targets.
     * If extremum is not null, the max and min value of targets will be
     * stored into it.
     */
    size_type find_exist_target(size_type s,
                                char_type *targets,
                                extremum_type *extremum) const
    {
        char_type ch;
        char_type *p;

        for (ch = 1, p = targets; ch < key_type::kCharsetSize + 1; ch++) {
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
    /// Pointer to header.
    header_type *header_;

    /// Pointer to state buffer.
    state_type *states_;

    /// Last avaiable BASE value.
    size_type last_base_;

    /// Number of state being used.
    size_type max_state_;

    /// Ownership of data.
    bool owner_;

    /// Relocator for notifying state changing.
    trie_relocator_interface<size_type> *relocator_;

    /// @see compact_header().
    mutable header_type compact_header_;
};

/**
 * Represents an relocator adaptor @see trie_relocator_interface.
 *
 * @param T Type of trie
 */
template<typename T>
class trie_relocator: public trie_relocator_interface<size_type> {
  public:
    /// Represents a callback function.
    typedef void (T::*relocate_function)(size_type, size_type);

    /**
     * Constructs a trie_relocator.
     *
     * @param who Pointer to a host trie.
     * @param relocate Pointer to a relocate function.
     */
    // XXX: change who to const ?
    trie_relocator(T *who, relocate_function relocate)
        :who_(who), relocate_(relocate)
    {
    }

    void relocate(size_type s, size_type t)
    {
        (who_->*relocate_)(s, t);
    }

  private:
    /// Pointer to a host trie.
    T *who_;

    /// Pointer to relocate function.
    relocate_function relocate_;

    /// Constructs a copy of trie_relocator.
    trie_relocator(const trie_relocator &);

    /// Updates a trie_relocator.
    void operator=(const trie_relocator &);
};

/**
 * Represents a two-trie structure.
 */
class double_trie: public trie_interface {
  public:
    /**
     * Represents some information about double_trie.
     */
    typedef struct {
        char magic[16];  ///< Archive magic.
        size_type index_size;  ///< Index array size.
        size_type accept_size; ///< Accept array size.
        char unused[40]; /// for 32/64bits compatible.
    } header_type;

    /**
     * Constructs a double_trie.
     *
     * @param size Initial size of state buffer.
     */
    explicit double_trie(size_t size = basic_trie::kDefaultStateSize);

    /**
     * Constructs a double_trie using a trie archive.
     *
     * @param filename Filename of the archive.
     */
    explicit double_trie(const char *filename);
    
    /// Destructs a double_trie.
    ~double_trie();

    void insert(const key_type &key, const value_type &value);
    bool search(const key_type &key, value_type *value) const;
    size_t prefix_search(const key_type &key, result_type *result) const;
    void build(const char *filename, bool verbose = false);

    /// Returns a pointer to front trie.
    const basic_trie *front_trie() const
    {
        return lhs_;
    }

    /// Returns a pointer to rear trie.
    const basic_trie *rear_trie() const
    {
        return rhs_;
    }

    /// Prints debug information.
    void trace_table(size_type istart,
                     size_type astart) const
    {
        static const size_type dsize = 20;
        size_type i;
        fprintf(stderr, "========================================");
        fprintf(stderr, "\nSEQ     |");
        for (i = istart; i < dsize && i < header_->index_size; i++)
            fprintf(stderr, "%4d ", i);
        fprintf(stderr, "\nDATA    |");
        for (i = istart; i < dsize && i < header_->index_size; i++)
            fprintf(stderr, "%4d ", index_[i].data);
        fprintf(stderr, "\nINDEX   |");
        for (i = istart; i < dsize && i < header_->index_size; i++)
            fprintf(stderr, "%4d ", index_[i].index);
        fprintf(stderr, "\nCOUNT   |");
        for (i = astart; i < dsize && i < header_->accept_size; i++)
            fprintf(stderr, "%4d ", count_referer(accept_[i].accept));
        fprintf(stderr, "\nACCEPT  |");
        for (i = astart; i < dsize && i < header_->accept_size; i++)
            fprintf(stderr, "%4d ", accept_[i].accept);
        fprintf(stderr, "\n========================================\n");
        std::set<size_type>::const_iterator it;
        std::map<size_type, refer_type>::const_iterator mit;
        for (mit = refer_.begin(); mit != refer_.end(); mit++) {
            fprintf(stderr, "%4d: ", mit->first);
            for (it = mit->second.referer.begin();
                 it != mit->second.referer.end();
                 it++)
                fprintf(stderr, "%4d ", *it);
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "========================================\n");
    }

  protected:
    /// Appends inputs to rear trie.
    size_type rhs_append(const char_type *inputs);

    /**
     * Inserts inputs into front trie.
     * 
     * @param s Mismatch state.
     * @param inputs Buffer of char_type to be inserted.
     * @param value Value for current key.
     */
    void lhs_insert(size_type s, const char_type *inputs, value_type value);
    
    /// cleans all unused states in rear trie from state t.
    void rhs_clean_more(size_type t);

    /**
     * Inserts into rear trie.
     *
     * @param s Mismatch state.
     * @param r Accept state.
     * @param match Common string between new key and existing one in front
     *              trie.
     * @param ch Mismatch char_type of the existing key.
     * @param value Value for current key.
     */
    void rhs_insert(size_type s, size_type r,
                    const std::vector<char_type> &match,
                    const char_type *remain, char_type ch, size_type value);

    /// Removes a accept state.
    void remove_accept_state(size_type s)
    {
        assert(s > 0);
        rhs_->set_base(s, 0);
        rhs_->set_check(s, 0);
        free_accept_entry(s);
    }

    /// Returns true if state s is a separated state.
    bool check_separator(size_type s) const
    {
        return (lhs_->base(s) < 0)?true:false;
    }

    /// Returns a accept state of a given separated state.
    size_type link_state(size_type s) const
    {
        return accept_[index_[-lhs_->base(s)].index].accept;
    }

    /**
      * Sets a accept state for a separated state.
      *
      * @param s The separated state.
      * @param t The accept state.
      */
    size_type set_link(size_type s, size_type t)
    {
        size_type i;

        if (refer_.find(t) != refer_.end() && refer_[t].referer.size()) {
            i = find_index_entry(s);
            index_[i].index = refer_[t].accept_index;
        } else {
            i = find_index_entry(s);
            size_type acc = find_accept_entry(i);
            accept_[acc].accept = t;
            assert(acc > 0 && acc < header_->accept_size);
            refer_[t].accept_index = acc;
        }
        assert(lhs_->base(s) < 0);
        assert(refer_.find(t) != refer_.end());
        refer_[t].referer.insert(s);

        return i;
    }

    /// Returns how many separated state linked to accept state s.
    size_t count_referer(size_type s) const
    {
        std::map<size_type, refer_type>::const_iterator found(refer_.find(s));
        if (found == refer_.end())
            return 0;
        else
            return found->second.referer.size();
    }

    /// Returns a free index entry and Updates state s to it.
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
                index_ = resize(index_, header_->index_size, nsize);
                assert(index_[next].index == 0);
                header_->index_size = nsize;
            }
            lhs_->set_base(s, -next);
        }
        return -lhs_->base(s);
    }

    /**
     * Returns a free accept entry and updates the (i)th
     * index's accept state to it.
     */
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
                accept_ = resize(accept_, header_->accept_size, nsize);
                header_->accept_size = nsize;
            }
            index_[i].index = next;
        }
        return index_[i].index;
    }

    /// Returns the out degree of state s in rear trie.
    size_t outdegree(size_type s) const
    {
        char_type ch;
        size_t degree = 0;
        for (ch = 1; ch < key_type::kCharsetSize + 1; ch++) {
            size_type t = rhs_->next(s, ch);
            if (t >= rhs_->header()->size)
                break;
            if (rhs_->check_transition(s, t))
                degree++;
        }

        return degree;
    }

    /**
     * Tries to remove an unused state in trie.
     *
     * @param t The state to be removed.
     * @return true if it is removed.
     */
    bool rhs_clean_one(size_type t)
    {
        assert(rhs_->check(t) > 0);
        size_type s = rhs_->prev(t);
        if (s > 0
            && t == rhs_->next(s, key_type::kTerminator)
            && count_referer(t) == 0) {
            // delete one
            remove_accept_state(t);
            return true;
        }
        return false;
    }

    /**
     * Fixes some index when there is state changing in front trie.
     *
     * @param s Original state.
     * @param t Target state.
     */
    void relocate_front(size_type s, size_type t)
    {
        /* It needs to check index_[-lhs_->base(s)].index > 0
         * 'cause we will store a zero in index to indicate
         * the searching key has no accept state but its
         * value has been stored into the index table
         */
        if (lhs_->base(s) < 0 && index_[-lhs_->base(s)].index > 0) {
            size_type r = link_state(s);
            if (refer_.find(r) != refer_.end()) {
                refer_[r].referer.erase(s);
                assert(lhs_->base(t) < 0);
                refer_[r].referer.insert(t);
            }
        }
    }

    /**
     * Fixes some index when there is state changing in rear trie.
     *
     * @param s Original state.
     * @param t Target state.
     */
    void relocate_rear(size_type s, size_type t)
    {
        if (refer_.find(s) != refer_.end()) {
            accept_[refer_[s].accept_index].accept = t;
            refer_[t] = refer_[s];
            free_accept_entry(s);
        }
        if (watcher_[0] == s) {
            watcher_[0] = t;
        }
        if (watcher_[1] == s) {
            watcher_[1] = t;
        }
    }

    /// Free an unused accept entry.
    void free_accept_entry(size_type s)
    {
        if (refer_.find(s) != refer_.end()) {
            // XXX: check what cause the inequivalent
            if (s > 0 && count_referer(s) == 0) {
                if (refer_[s].accept_index < header_->accept_size) {
                    if (refer_[s].accept_index > 0) {
                        accept_[refer_[s].accept_index].accept = 0;
                        free_accept_.push_back(refer_[s].accept_index);
                    }
                }
            }
            refer_.erase(s);
        }
    }

  private:
    /// Represents a separated state index.
    typedef struct {
        size_type accept;
    } accept_type;

    /// Represents a index to accept_type.
    typedef struct {
        value_type data;
        size_type index;
    } index_type;

    /// Represents a back reference from accept state to
    /// separated state.
    typedef struct {
        size_type accept_index;
        std::set<size_type> referer;
    } refer_type;

    /// Pointer to header.
    header_type *header_;
    
    /// Pointer to front trie(lhs_) and rear trie(rhs_).
    basic_trie *lhs_, *rhs_;

    /// Pointer to index to accept_type index.
    index_type *index_;

    /// Pointer to accept_type index.
    accept_type *accept_;

    /// Accept state back reference.
    std::map<size_type, refer_type> refer_;

    /// Temporary buffer for storing exising char_types while inserting.
    std::vector<char_type> exists_;

    /// Next available entry in accept_/index_.
    size_type next_accept_, next_index_;

    /// Relocator for front and rear trie.
    trie_relocator<double_trie> *front_relocator_, *rear_relocator_;

    /// States to be monitored by relocator
    size_type watcher_[2];

    /// List of freed accept entry.
    std::deque<size_type> free_accept_;

    /// List of freed index entry.
    std::deque<size_type> free_index_;

    /// Pointer to mmapped buffer
    void *mmap_;

    /// Length of mmapped buffer
    size_t mmap_size_;

    /// Archive magic.
    static const char magic_[16];
};

/**
 * Represents a tail-trie structure.
 */
class single_trie: public trie_interface
{
  public:
    /// Represents an element in suffix buffer.
    typedef size_type suffix_type;

    /**
     * Represents some information about single_trie.
     */
    typedef struct {
        char magic[16];  ///< Archive magic.
        size_type suffix_size;  ///< Size of suffix buffer.
        char unused[44];  ///< for 32/64 bits compatible.
    } header_type;

    /**
     * Represents a resizable buffer for storing the common part of
     * newly inserting key and an existing one.
     */
    typedef struct {
        char_type *data; ///< Data buffer.
        size_t size;     ///< Buffer size.
    } common_type;

    /// Default size of common_
    static const size_t kDefaultCommonSize = 256;

    /**
     * Constructs an empty single_trie.
     *
     * @param s Initial size of state.
     */
    // XXX: should default size be kDefaultStateSize?
    explicit single_trie(size_t size = 0);

    /**
     * Constructs an single_trie from archive.
     *
     * @param filename Filename of the archive.
     */
    explicit single_trie(const char *filename);

    /// Destructs a single_trie.
    ~single_trie();

    void insert(const key_type &key, const value_type &value);
    bool search(const key_type &key, value_type *value) const;
    size_t prefix_search(const key_type &key, result_type *result) const;
    void build(const char *filename, bool verbose);

    /// Returns a pointer to the trie of single_trie.
    const basic_trie *trie()
    {
        return trie_;
    }

    /// Returns a pointer to the tail of single_trie.
    const suffix_type *suffix()
    {
        return suffix_;
    }

    /// Prints debug information about suffix.
    void trace_suffix(size_type start, size_type count) const
    {
        size_type i;
        for (i = start; i < header_->suffix_size && i < count; i++) {
            if (suffix_[i] == key_type::kTerminator)
                fprintf(stderr, "[%d:#]", i);
            else if (isgraph(key_type::char_out(suffix_[i])))
                fprintf(stderr, "[%d:%c]",
                        i, key_type::char_out(suffix_[i]));
            else
                fprintf(stderr, "[%d:%x]", i, suffix_[i]);
        }
        printf("\n");
    }

  protected:
    /**
     * Resizes suffix to expected size
     *
     * @param size Expected size.
     */
    void resize_suffix(size_type size)
    {
        // align with 4k
        size_type nsize = (((header_->suffix_size * 2 + size) >> 12) + 1) << 12;
        suffix_ = resize(suffix_, header_->suffix_size, nsize);
        header_->suffix_size = nsize;
    }

    /**
     * Resizes common to expected size
     *
     * @param size Expected size.
     */
    void resize_common(size_type size)
    {
        // align with 4k
        size_type nsize = (((common_.size * 2 + size) >> 12) + 1) << 12;
        common_.data = resize(common_.data, common_.size, nsize);
        common_.size = nsize;
    }

    /**
     * Inserts inputs into suffix.
     *
     * @param s Separated state in trie
     * @param inputs The inputs
     * @param value The value
     */
    void insert_suffix(size_type s, const char_type *inputs, value_type value);

    /**
     * Creates a branch in trie. This function creates a branch in trie
     * when there are common part between newly inserting key and an
     * existing one.
     *
     * @param s Separated state in trie
     * @param inputs The inputs
     * @param value The value
     */
    void create_branch(size_type s, const char_type *inputs, value_type value);

  private:
    /// Pointer to trie
    basic_trie *trie_;

    /// Pointer to suffix
    suffix_type *suffix_;

    /// Pointer to header
    header_type *header_;

    /// Next available suffix
    size_type next_suffix_;

    /**
     * Temporary buffer to store common part betwee newly 
     * inserting key and an existing one
     */
    common_type common_;

    void *mmap_;
    size_t mmap_size_;

    /// Archive magic
    static const char magic_[16];
};
#endif  // TRIE_IMPL_H_

END_TRIE_NAMESPACE

// vim: ts=4 sw=4 ai et
