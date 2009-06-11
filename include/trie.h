#ifndef TRIE_H_
#define TRIE_H_

#include <cstdlib>
#include <stdexcept>

#define BEGIN_TRIE_NAMESPACE namespace trie {
#define END_TRIE_NAMESPACE }

BEGIN_TRIE_NAMESPACE

typedef int32_t value_type;
typedef int32_t size_type;
typedef int32_t char_type;
enum trie_type {UNKNOW = 0,	SINGLE_TRIE, DOUBLE_TRIE};

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

	key_type(): data_(NULL), capacity_(0) {}
	explicit key_type(const char *data, size_t length)
        :data_(NULL), capacity_(0)
	{
        assign(data, length);
	}

    ~key_type()
    {
		free(data_);
        capacity_ = 0;
    }

    const char_type *data() const
    {
        return data_;
    }

    static char_type char_in(const char ch)
    {
        return static_cast<unsigned char>(ch + 1);
    }

    static char char_out(char_type ch)
    {
        return static_cast<char>(ch - 1);
    }

    void assign(const char *data, size_t length)
    {
        size_t i;
        if (length + 1 >= capacity_) {
			size_t nsize = (capacity_ + length + 1) * 2;
			data_ = static_cast<char_type *>
			        (realloc(data_, nsize * sizeof(char_type)));
			capacity_ = nsize;
		}
        for (i = 0; i < length; i++)
            data_[i] = char_in(data[i]);
        data_[i] = kTerminator;
    }

  private:
  	char_type *data_;
	size_t capacity_;
};

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
