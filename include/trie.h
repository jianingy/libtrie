#ifndef TRIE_H_
#define TRIE_H_

#include <stdexcept>

#define BEGIN_TRIE_NAMESPACE namespace trie {
#define END_TRIE_NAMESPACE }

BEGIN_TRIE_NAMESPACE

class trie_interface {
  public:
    typedef int32_t value_type;
    typedef int32_t size_type;
    typedef int32_t char_type;

    trie_interface() {}
    explicit trie_interface(size_t size) {}
    explicit trie_interface(const char *filename) {}
    virtual void insert(const char *inputs, size_t length,
                        value_type value) = 0;
    virtual bool search(const char *inputs, size_t length,
                        value_type *value) const = 0;
    virtual void build(const char *filename, bool verbose = false) = 0;
	virtual void read_from_text(const char *source, bool verbose = false);
    virtual ~trie_interface() = 0;
};

class bad_trie_archive: public std::runtime_error {
  public:
	explicit bad_trie_archive(const char *s):std::runtime_error(s) {}
};

class bad_trie_source: public std::runtime_error {
  public:
	explicit bad_trie_source(const char *s):std::runtime_error(s) {}
};

enum trie_type {
	UNKNOW = 0,
	SINGLE_TRIE,
	DOUBLE_TRIE
};

/* create an empty trie */
trie *create_trie(trie_type type = DOUBLE_TRIE, size_t size = 4096);
/* create a trie using archive */
trie *create_trie(const char *archive);

END_TRIE_NAMESPACE

#endif  // TRIE_H_
