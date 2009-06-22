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

/**
 * Represents the value to be stored in double-array.
 */
typedef int32_t value_type;

/**
 * Represents the size/index of double-array.
 */
typedef int32_t size_type;

/**
 * Represents the transition character.
 */
typedef int32_t char_type;

/**
 * Represents the type of trie.
 */
enum trie_type {
    UNKNOW = 0,   /**< Unknow. */
    SINGLE_TRIE,  /**< Tail Trie. */
    DOUBLE_TRIE   /**< Two Trie. */
};

/**
 * Exception indicates errors while operating on
 * trie's index file.
 */
class bad_trie_archive: public std::runtime_error {
  public:
    explicit bad_trie_archive(const char *s):std::runtime_error(s) {}
};

/**
 * Exception indicates errors while building trie from
 * a formatted text file.
 */
class bad_trie_source: public std::runtime_error {
  public:
    explicit bad_trie_source(const char *s):std::runtime_error(s) {}
};

/**
 * Represents the key to be stored in double-array.
 */
class key_type {
  public:
    /**
     * The size of charset. 
     */
    static const char_type kCharsetSize = 257;

    /**
     * Terminator character (should not in charset).
     */
    static const char_type kTerminator = kCharsetSize;

    /**
     * Default constructor.
     */
    key_type()
        :cstr_(NULL), cstr_capacity_(0),
         data_(NULL), data_capacity_(0),
         length_(0)
    {}
    
    /**
     * Construct from a c-style data.
     * @param data Pointer to the c-style data.
     * @param length Length of the c-style data.
     */
    explicit key_type(const char *data, size_t length)
        :cstr_(NULL), cstr_capacity_(0),
         data_(NULL), data_capacity_(0),
         length_(0)
    {
        assign(data, length);
    }

    /**
     * Copy constructor.
     * @param key Const reference to a key_type object.
     */
    explicit key_type(const key_type &key)
        :cstr_(NULL), cstr_capacity_(0),
         data_(NULL), data_capacity_(0),
         length_(0)
    {
        assign(key.data(), key.length());
    }

    /**
     * Assignment operator.
     * @param key Const reference to a key object.
     */
    const key_type &operator=(const key_type &rhs)
    {
        assign(rhs.data(), rhs.length());
        return *this;
    }

    /**
     * Destructor.
     */
    ~key_type()
    {
        free(data_);
        free(cstr_);
        data_capacity_ = 0;
        cstr_capacity_ = 0;
    }

    /**
     * Return a const pointer to internal data.
     */
    const char_type *data() const
    {
        return data_;
    }

    /**
     * Return the length of internal data.
     */
    size_t length() const
    {
        return length_;
    }

    /**
     * Convert char to transition char.
     * @param ch char to be converted.
     * @return Converted result.
     */
    static char_type char_in(const char ch)
    {
        return static_cast<unsigned char>(ch + 1);
    }

    /**
     * Convert transition char to char.
     * @param ch char_type character to be converted.
     * @return Converted result.
     */
    static char char_out(char_type ch)
    {
        return static_cast<char>(ch - 1);
    }

    /**
     * Add a character at the end
     * @param ch char to be pushed.
     */
    void push(char_type ch)
    {
        if (length_ + 1 >= data_capacity_)
            resize_data(1);
        data_[length_++] = ch;
        data_[length_] = kTerminator;
    }

    /**
     * Remove the character at the end return the char.
     * @return The character at the end.
     */
    char_type pop()
    {
        char_type ch;
        ch = data_[length_--];
        data_[length_] = kTerminator;
        return ch;
    }

    /**
     * Empty data buffer.
     */
    void clear()
    {
        data_[0] = kTerminator;
        length_ = 0;
    }

    /**
     * Convert data buffer to c-style data buffer and return it.
     * @return Pointer to the c-style data buffer.
     */
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

    /**
     * Assign from c-style data.
     * @param data The data to be assigned.
     * @param length Length of the data.
     */
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

    /**
     * Assign from a transition char string.
     * @param data The data to be assigned.
     * @param length Length of the data.
     */
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
    /**
     * Resize internal data buffer
     * @param size Expected size
     */
    void resize_data(size_t size)
    {
        size_t nsize = (data_capacity_ + size + 1) * 2;
        data_ = static_cast<char_type *>
            (realloc(data_, nsize * sizeof(char_type)));
        data_capacity_ = nsize;
    }

    /**
     * Resize c-style data buffer for converting
     */
    void resize_cstr() const
    {
        cstr_ = static_cast<char *>
            (realloc(cstr_, data_capacity_ * sizeof(char)));
        cstr_capacity_ = data_capacity_;
    }

  private:
    /**
     * C-style data buffer for converting
     */
    mutable char *cstr_;
    /**
     * Size of cstr_ buffer
     */
    mutable size_t cstr_capacity_;
    /**
     * data buffer
     */
    char_type *data_;
    /**
     * Size of data_ buffer
     */
    size_t data_capacity_;
    /**
     * Length of data in internal data buffer
     */
    size_t length_;
};

/**
 * Represent the result set for prefix_search
 */
typedef std::vector<std::pair<key_type, value_type> > result_type;

/**
 * Interface for manipulating different trie structure.
 */
class trie_interface {
  public:
    /**
     * Default constructor.
     */
    trie_interface() {}

    /**
     * construct by a specified size.
     * @param size The initial size of states.
     */
    explicit trie_interface(size_t size) {}

    /**
     * Construct from a specified archive file.
     * @param filename Filename of the archive file.
     */
    explicit trie_interface(const char *filename) {}

    /**
     * Insert an element into trie.
     * @param key Key to the element.
     * @param value Value of the element.
     */
    virtual void insert(const key_type &key, const value_type &value) = 0;

    /**
     * Retrieve value of element by given key.
     * @param key Key to the element.
     * @param value Value of the element.
     * @return true if found.
     */
    virtual bool search(const key_type &key, value_type *value) const = 0;

    /**
     * Insert an element into trie.
     * @param inputs Key to the element represented by a c-style string.
     * @param length Length of the key.
     * @param value Value of the element.
     */
    virtual void insert(const char *inputs, size_t length,
                        value_type value);

    /**
     * Retrieve value of element by given key.
     * @param inputs Key to the element represented by a c-style string.
     * @param length Length of the key.
     * @param value Value of the element.
     * @return true if found.
     */
    virtual bool search(const char *inputs, size_t length,
                        value_type *value) const;

    /**
     * Retrieve all keys start with given prefix.
     * @param key The given prefix.
     * @param result Result set.
     * @return Size of the result set.
     */
    virtual size_t prefix_search(const key_type &key,
                                 result_type *result) const = 0;
    /**
     * Build a trie archive with current data.
     * @param filename Filename of the archive.
     * @param verbose Display detail information while building
     *                if it sets to true.
     */
    virtual void build(const char *filename, bool verbose = false) = 0;

    /**
     * Build a trie from a formatted text file.
     * @param source Filename of the text file.
     * @param verbose Display detail information while reading
     *                if it sets to true.
     */
    virtual void read_from_text(const char *source, bool verbose = false);

    /**
     * Destructor
     */
    virtual ~trie_interface() = 0;
};

/**
 * Create an empty trie.
 * @param trie_type The type of trie to be create.
 * @param size The initial size of trie. This is not an accurate value but
 *             a suggestion. Increase/Decrease this value according to
 *             the size of your data.
 */
trie_interface *create_trie(trie_type type = DOUBLE_TRIE, size_t size = 4096);

/** 
 * Create a trie from disk archive.
 * @param archive The filename of the archive.
 */
trie_interface *create_trie(const char *archive);

END_TRIE_NAMESPACE

#endif  // TRIE_H_

// vim: ts=4 sw=4 ai et
