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
#ifndef TRIE_H_
#define TRIE_H_

#include <map>
#include <vector>
#include <cstdlib>
#include <stdexcept>

#define BEGIN_TRIE_NAMESPACE namespace trie {
#define END_TRIE_NAMESPACE }

BEGIN_TRIE_NAMESPACE

typedef int32_t value_type;
typedef int32_t size_type;
typedef int32_t char_type;
enum trie_type {UNKNOW = 0, SINGLE_TRIE, DOUBLE_TRIE};

class bad_trie_archive: public std::runtime_error {
  public:
    explicit bad_trie_archive(const char *s):std::runtime_error(s) {}
};

class bad_trie_source: public std::runtime_error {
  public:
    explicit bad_trie_source(const char *s):std::runtime_error(s) {}
};

class key_type {
  public:
    static const char_type kCharsetSize = 257;
    static const char_type kTerminator = kCharsetSize;

    key_type()
        :cstr_(NULL), cstr_capacity_(0),
         data_(NULL), data_capacity_(0),
         length_(0)
    {}

    explicit key_type(const char *data, size_t length)
        :cstr_(NULL), cstr_capacity_(0),
         data_(NULL), data_capacity_(0),
         length_(0)
    {
        assign(data, length);
    }

    explicit key_type(const key_type &key)
        :cstr_(NULL), cstr_capacity_(0),
         data_(NULL), data_capacity_(0),
         length_(0)
    {
        assign(key.data(), key.length());
    }

    const key_type &operator=(const key_type &rhs)
    {
        assign(rhs.data(), rhs.length());
        return *this;
    }

    ~key_type()
    {
        free(data_);
        free(cstr_);
        data_capacity_ = 0;
        cstr_capacity_ = 0;
    }

    const char_type *data() const
    {
        return data_;
    }

    size_t length() const
    {
        return length_;
    }

    static char_type char_in(const char ch)
    {
        return static_cast<unsigned char>(ch + 1);
    }

    static char char_out(char_type ch)
    {
        return static_cast<char>(ch - 1);
    }

    void push(char_type ch)
    {
        if (length_ + 1 >= data_capacity_)
            resize_data(1);
        data_[length_++] = ch;
        data_[length_] = kTerminator;
    }

    char_type pop()
    {
        char_type ch;
        ch = data_[length_--];
        data_[length_] = kTerminator;
        return ch;
    }

    void clear()
    {
        data_[0] = kTerminator;
        length_ = 0;
    }

    const char *c_str() const
    {
        size_t i;
        if (cstr_capacity_ < data_capacity_)
            resize_cstr();
        for (i = 0; data_[i] != kTerminator ; i++)
            cstr_[i] = char_out(data_[i]);
        cstr_[i] = '\0';
        return cstr_;
    }

    void assign(const char *data, size_t length)
    {
        size_t i;
        if (length + 1 >= data_capacity_)
            resize_data(length);
        for (i = 0; i < length; i++)
            data_[i] = char_in(data[i]);
        data_[i] = kTerminator;
        length_ = length;
    }

    void assign(const char_type *data, size_t length)
    {
        size_t i;
        if (length + 1 >= data_capacity_)
            resize_data(length);
        for (i = 0; i < length; i++)
            data_[i] = data[i];
        data_[i] = kTerminator;
        length_ = length;
    }

  protected:
    void resize_data(size_t size)
    {
        size_t nsize = (data_capacity_ + size + 1) * 2;
        data_ = static_cast<char_type *>
            (realloc(data_, nsize * sizeof(char_type)));
        data_capacity_ = nsize;
    }

    void resize_cstr() const
    {
        cstr_ = static_cast<char *>
            (realloc(cstr_, data_capacity_ * sizeof(char)));
        cstr_capacity_ = data_capacity_;
    }

  private:
    mutable char *cstr_;
    mutable size_t cstr_capacity_;
    char_type *data_;
    size_t data_capacity_;
    size_t length_;
};

typedef std::vector<std::pair<key_type, value_type> > result_type;

class trie_interface {
  public:
    trie_interface() {}
    explicit trie_interface(size_t size) {}
    explicit trie_interface(const char *filename) {}
    virtual void insert(const key_type &key, const value_type &value) = 0;
    virtual bool search(const key_type &key, value_type *value) const = 0;
    virtual void insert(const char *inputs, size_t length,
                        value_type value);
    virtual bool search(const char *inputs, size_t length,
                        value_type *value) const;
    virtual size_t prefix_search(const key_type &key,
                                 result_type *result) const = 0;
    virtual void build(const char *filename, bool verbose = false) = 0;
    virtual void read_from_text(const char *source, bool verbose = false);
    virtual ~trie_interface() = 0;
};

/* create an empty trie */
trie_interface *create_trie(trie_type type = DOUBLE_TRIE, size_t size = 4096);
/* create a trie using archive */
trie_interface *create_trie(const char *archive);

END_TRIE_NAMESPACE

#endif  // TRIE_H_

// vim: ts=4 sw=4 ai et
