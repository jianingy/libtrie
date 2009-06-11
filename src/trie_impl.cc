// Copyright Jianing Yang <jianingy.yang@gmail.com> 2009

#include <iostream>
#include <cstdio>

#include "trie_impl.h"

#define sanity_delete(X)  do { \
    if (X) { \
        delete (X); \
        (X) = NULL; \
    }} while (0);

BEGIN_TRIE_NAMESPACE

const char double_trie::magic_[16] = "TWO_TRIE";
const char single_trie::magic_[16] = "TAIL_TRIE";

// ************************************************************************
// * Implementation of helper functions                                   *
// ************************************************************************

static const char* pretty_size(size_t size, char *buf, size_t buflen)
{
    assert(buf);
    if (size > 1024 * 1024 * 1024) {
        snprintf(buf, buflen, "%4.2fG",
                 static_cast<double>(size) / (1024 * 1024 * 1024));
    } else if (size > 1024 * 1024) {
        snprintf(buf, buflen, "%4.2fM",
                 static_cast<double>(size) / (1024 * 1024));
    } else if (size > 1024) {
        snprintf(buf, buflen, "%4.2fK", static_cast<double>(size) / 1024);
    } else {
        snprintf(buf, buflen, "%4.2f", static_cast<double>(size));
    }
    return buf;
}

trie_interface::~trie_interface()
{
}

// ************************************************************************
// * Implementation of basic_trie                                         *
// ************************************************************************

basic_trie::basic_trie(size_type size,
                       trie_relocator_interface<size_type> *relocator)
    :header_(NULL), states_(NULL), last_base_(0), max_state_(0), owner_(true),
     relocator_(relocator)
{
    if (size < key_type::kCharsetSize)
        size = kDefaultStateSize;
    header_ = new header_type();
    memset(header_, 0, sizeof(header_type));
    resize_state(size);
}

basic_trie::basic_trie(void *header, void *states)
    :header_(NULL), states_(NULL), last_base_(0), max_state_(0), owner_(false),
     relocator_(NULL)
{
    header_ = static_cast<header_type *>(header);
    states_ = static_cast<state_type *>(states);
}

basic_trie::basic_trie(const basic_trie &trie)
    :header_(NULL), states_(NULL), last_base_(0), max_state_(0), owner_(false),
     relocator_(NULL)
{
    clone(trie);
}

basic_trie &basic_trie::operator=(const basic_trie &trie)
{
    clone(trie);
    return *this;
}

void basic_trie::clone(const basic_trie &trie)
{
    if (owner_) {
        if (header_) {
            sanity_delete(header_);
        }
        if (states_) {
            resize(states_, 0, 0);
            states_ = NULL;  // set to NULL for next resize
        }
    }
    owner_ = true;
    max_state_ = trie.max_state();
    header_ = new header_type();
    states_ = resize(states_, 0, trie.header()->size);
    memcpy(header_, trie.header(), sizeof(header_type));
    memcpy(states_, trie.states(), trie.header()->size * sizeof(state_type));
}

basic_trie::~basic_trie()
{
    if (owner_) {
        sanity_delete(header_);
        resize(states_, 0, 0);  // free states_
    }
}

size_type
basic_trie::find_base(const char_type *inputs, const extremum_type &extremum)
{
    bool found;
    size_type i;
    const char_type *p;

    for (i = last_base_, found = false; !found; /* empty */) {
        i++;
        if (i + extremum.max >= header_->size)
            resize_state(extremum.max);
        if (check(i + extremum.min) <= 0 && check(i + extremum.max) <= 0) {
            for (p = inputs, found = true; *p; p++) {
                if (check(i + *p) > 0) {
                    found = false;
                    break;
                }
            }
        } else {
            //  i += extremum.min;
        }
    }

    last_base_ = i;

    return last_base_;
}

size_type
basic_trie::relocate(size_type stand,
                     size_type s,
                     const char_type *inputs,
                     const extremum_type &extremum)
{
    size_type obase, nbase, i;
    char_type targets[key_type::kCharsetSize + 1];

    obase = base(s);  // save old base value
    nbase = find_base(inputs, extremum);  // find a new base

    for (i = 0; inputs[i]; i++) {
        if (check(obase + inputs[i]) != s)  // find old links
            continue;
        set_base(nbase + inputs[i], base(obase + inputs[i]));
        set_check(nbase + inputs[i], check(obase + inputs[i]));
        find_exist_target(obase + inputs[i], targets, NULL);
        for (char_type *p = targets; *p; p++) {
            set_check(base(obase + inputs[i]) + *p, nbase + inputs[i]);
        }
        // if where we are standing is moving, we move with it
        if (stand == obase + inputs[i])
            stand = nbase + inputs[i];
        if (relocator_)
            relocator_->relocate(obase + inputs[i], nbase + inputs[i]);
        // free old places
        set_base(obase + inputs[i], 0);
        set_check(obase + inputs[i], 0);
        // create new links according old ones
    }
    // finally, set new base
    set_base(s, nbase);

    return stand;
}

size_type
basic_trie::create_transition(size_type s, char_type ch)
{
    char_type targets[key_type::kCharsetSize + 1];
    char_type parent_targets[key_type::kCharsetSize + 1];
    extremum_type extremum = {0, 0}, parent_extremum = {0, 0};

    size_type t = next(s, ch);
    if (t >= header_->size)
        resize_state(t - header_->size + 1);

    if (base(s) > 0 && check(t) <= 0) {
        // Do Nothing !!
    } else {
        size_type num_targets =
            find_exist_target(s, targets, &extremum);
        size_type num_parent_targets = check(t)?
            find_exist_target(check(t), parent_targets, &parent_extremum):0;
        if (num_parent_targets > 0 && num_targets + 1 > num_parent_targets) {
            s = relocate(s, check(t), parent_targets, parent_extremum);
        } else {
            targets[num_targets++] = ch;
            targets[num_targets] = 0;
            if (ch > extremum.max || !extremum.max)
                extremum.max = ch;
            if (ch < extremum.min || !extremum.min)
                extremum.min = ch;
            s = relocate(s, s, targets, extremum);
        }
        t = next(s, ch);
        if (t >= header_->size)
            resize_state(t - header_->size + 1);
    }
    set_check(t, s);

    return t;
}


void basic_trie::insert(const key_type &key, const value_type &value)
{
    if (value < 1)
        throw std::runtime_error("basic_trie::insert: value must > 0");

    const char_type *p = NULL;
    size_type s = go_forward(1, key.data(), &p);
    do {
        s = create_transition(s, *p);
    } while (*p++ != key_type::kTerminator);
    set_base(s, value);
}


bool basic_trie::search(const key_type &key, value_type *value) const
{
    const char_type *p = NULL;
    size_type s = go_forward(1, key.data(), &p);
    if (p)
        return false;
    if (value)
        *value = base(s);
    return true;
}

void basic_trie::trace(size_type s) const
{
    size_type num_target;
    char_type targets[key_type::kCharsetSize + 1];
    static std::vector<size_type> trace_stack;

    trace_stack.push_back(s);
    if ((num_target = find_exist_target(s, targets, NULL))) {
        for (char_type *p = targets; *p; p++) {
            size_type t = next(s, *p);
            if (t < header_->size)
                trace(next(s, *p));
        }
    } else {
        size_type cbase = 0, obase = 0;
        std::cerr << "transition => ";
        std::vector<size_type>::const_iterator it;
        for (it = trace_stack.begin();it != trace_stack.end(); it++) {
            cbase = base(*it);
            if (obase) {
                if (*it - obase == key_type::kTerminator) {
                    std::cerr << "-#->";
                } else {
                    char ch = key_type::char_out(*it - obase);
                    if (isgraph(ch))
                        std::cerr << "-'" << ch << "'->";
                    else
                        std::cerr << "-<" << std::hex
                                  << static_cast<uint8_t>(ch) << ">->";
                }
            }
            std::clog << *it << "[" << cbase << "]";
            obase = cbase;
        }
        std::cerr << "->{" << std::dec << (cbase) << "}" << std::endl;
    }
    trace_stack.pop_back();
}

// ************************************************************************
// * Implementation of two trie                                           *
// ************************************************************************

double_trie::double_trie(size_t size)
    :header_(NULL), lhs_(NULL), rhs_(NULL), index_(NULL), accept_(NULL),
     next_accept_(1), next_index_(1), front_relocator_(NULL),
     rear_relocator_(NULL), stand_(0), mmap_(NULL), mmap_size_(0)
{
    header_ = new header_type();
    memset(header_, 0, sizeof(header_type));
    snprintf(header_->magic, sizeof(header_->magic), "%s", magic_);
    front_relocator_ = new trie_relocator<double_trie>
                           (this, &double_trie::relocate_front);
    rear_relocator_ = new trie_relocator<double_trie>
                          (this, &double_trie::relocate_rear);
    lhs_ = new basic_trie(size);
    rhs_ = new basic_trie(size);
    lhs_->set_relocator(front_relocator_);
    rhs_->set_relocator(rear_relocator_);
    header_->index_size = size?size:basic_trie::kDefaultStateSize;
    index_ = resize(index_, 0, header_->index_size);
    header_->accept_size = size?size:basic_trie::kDefaultStateSize;
    accept_ = resize(accept_, 0, header_->accept_size);
}

double_trie::double_trie(const char *filename)
    :header_(NULL), lhs_(NULL), rhs_(NULL), index_(NULL), accept_(NULL),
     next_accept_(1), next_index_(1), front_relocator_(NULL),
     rear_relocator_(NULL), stand_(0), mmap_(NULL), mmap_size_(0)
{
    struct stat sb;
    int fd, retval;

    if (!filename)
        throw std::runtime_error(std::string("can not load from file ")
                                 + filename);

    fd = open(filename, O_RDONLY);
    if (fd < 0)
        throw std::runtime_error(strerror(errno));
    if (fstat(fd, &sb) < 0)
        throw std::runtime_error(strerror(errno));

    mmap_ = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mmap_ == MAP_FAILED)
        throw std::runtime_error(strerror(errno));
    while (retval = close(fd), retval == -1 && errno == EINTR) {
        // exmpty
    }
    mmap_size_ = sb.st_size;

    void *start;
    start = header_ = reinterpret_cast<header_type *>(mmap_);
    if (strcmp(header_->magic, magic_))
        throw std::runtime_error("file corrupted");
    // load index
    start = index_ = reinterpret_cast<index_type *>(
                     reinterpret_cast<header_type *>(start) + 1);
    // load accept
    start = accept_ = reinterpret_cast<accept_type *>(
                      reinterpret_cast<index_type *>(start)
                      + header_->index_size);
    // load front trie
    start = reinterpret_cast<accept_type *>(start) + header_->accept_size;
    lhs_ = new basic_trie(start,
                          reinterpret_cast<basic_trie::header_type *>(start)
                          + 1);
    // load rear trie
    start = reinterpret_cast<basic_trie::state_type *>
            ((basic_trie::header_type *)start + 1)
            + lhs_->header()->size;
    rhs_ = new basic_trie(start,
                          reinterpret_cast<basic_trie::header_type *>(start)
                          + 1);
}


double_trie::~double_trie()
{
    if (mmap_) {
        if (munmap(mmap_, mmap_size_) < 0)
            throw std::runtime_error(strerror(errno));
    } else {
        sanity_delete(header_);
        resize(index_, 0, 0);  // free index_
        resize(accept_, 0, 0);  // free accept_
        sanity_delete(front_relocator_);
        sanity_delete(rear_relocator_);
    }
    sanity_delete(lhs_);
    sanity_delete(rhs_);
}

size_type
double_trie::rhs_append(const char_type *inputs)
{
    const char_type *p;
    size_type s = 1, t;

    s = rhs_->go_forward_reverse(s, inputs, &p);
    if (!p) {  // all characters match
        if (outdegree(s) == 0) {
            return s;
        } else {
            t = rhs_->next(s, key_type::kTerminator);
            if (!rhs_->check_transition(s, t))
                return rhs_->create_transition(s, key_type::kTerminator);
            return t;
        }
    }
    if (outdegree(s) == 0) {
        t = rhs_->create_transition(s, key_type::kTerminator);
        std::set<size_type>::const_iterator it;
        for (it = refer_[s].referer.begin();
                it != refer_[s].referer.end();
                it++) {
            set_link(*it, t);
        }
        free_accept_entry(s);
    }
    do {
        s = rhs_->create_transition(s, *p);
    } while (p-- > inputs);
    return s;
}

void
double_trie::lhs_insert(size_type s, const char_type *inputs, value_type value)
{
    // XXX: check inputs[0] == kTerminator for duplicated key
    s = lhs_->create_transition(s, inputs[0]);
    s = set_link(s, rhs_append(inputs + 1));
    index_[s].data = value;
}

void double_trie::rhs_clean_more(size_type t)
{
    assert(t > 0);
    if (outdegree(t) == 0 && count_referer(t) == 0) {
        size_type s = rhs_->prev(t);
        remove_accept_state(t);
        if (s > 0)
            rhs_clean_more(s);
    } else if (outdegree(t) == 1) {
        size_type r = rhs_->next(t, key_type::kTerminator);
        if (rhs_->check_transition(t, r)) {
            // delete transition 't -#-> r'
            std::set<size_type>::const_iterator it;
            for (it = refer_[r].referer.begin();
                    it != refer_[r].referer.end();
                    it++)
                set_link(*it, t);
            accept_[refer_[t].accept_index].accept = t;
            remove_accept_state(r);
        }
    }
}

void double_trie::rhs_insert(size_type s, size_type r,
                             const std::vector<char_type> &match,
                             const char_type *remain,
                             char_type ch, size_type value)
{
    // R-1
    size_type u = link_state(s);  // u might be zero
    value_type oval = index_[-lhs_->base(s)].data;
    index_[-lhs_->base(s)].index = 0;
    index_[-lhs_->base(s)].data = 0;
    free_index_.push_back(-lhs_->base(s));
// XXX: check out the crash reason if base(s) is not set to zero.
    lhs_->set_base(s, 0);
    stand_ = r;
    if (u > 0) {
        refer_[u].referer.erase(s);

        if (refer_[u].referer.size() == 0)
            free_accept_entry(u);
    }

    // R-2
    std::vector<char_type>::const_iterator it;
    for (it = match.begin(); it != match.end(); it++) {
        s = lhs_->create_transition(s, *it);
    }

    size_type t = lhs_->create_transition(s, *remain);
    // XXX: Check if remian + 1 exists, may related to duplicated key
    size_type i;
    if (*remain == key_type::kTerminator) {
        i = next_index_++;
        index_[i].data = value;
        lhs_->set_base(t, -i);
    } else {
        i = set_link(t, rhs_append(remain + 1));
        index_[i].data = value;
    }

    // R-3
    t = lhs_->create_transition(s, ch);
    size_type v = rhs_->prev(stand_);  // v -ch-> r
    if (!rhs_->check_transition(v, rhs_->next(v, key_type::kTerminator)))
        r = rhs_->create_transition(v, key_type::kTerminator);
    else
        r = rhs_->next(v, key_type::kTerminator);
    i = set_link(t, r);
    index_[i].data = oval;

    // R-4
    if (u > 0) {
        if (!rhs_clean_one(u))
            rhs_clean_more(u);
    }
}

void double_trie::insert(const key_type &key, const value_type &value)
{
    const char_type *p;
    size_type s = lhs_->go_forward(1, key.data(), &p);

    // XXX: check p == null for duplicated key
    if (!check_separator(s)) {
        lhs_insert(s, p, value);
        return;
    }
    if (!p) {
        index_[-lhs_->base(s)].data = value;
        return;
    }

    // skip dummy terminator
    size_type r = link_state(s);
    if (rhs_->check_reverse_transition(r, key_type::kTerminator)
        && rhs_->prev(r) > 1)
        r = rhs_->prev(r);

    // travel reversely
    exists_.clear();
    do {
        if (rhs_->check_reverse_transition(r, *p)) {
            r = rhs_->prev(r);
            exists_.push_back(*p);
        } else {
            break;
        }
    } while (*p++ != key_type::kTerminator);
    if (r == 1) {
        index_[-lhs_->base(s)].data = value;
        return;
    }
    char_type mismatch = r - rhs_->base(rhs_->prev(r));
    rhs_insert(s, r, exists_, p, mismatch, value);
    return;
}

bool double_trie::search(const key_type &key, value_type *value) const
{
    const char_type *p, *mismatch;
    size_type s = lhs_->go_forward(1, key.data(), &p);
    if (!check_separator(s))
        return false;
    if (!p) {
        if (value)
            *value = index_[-lhs_->base(s)].data;
        return true;
    }
    size_type r = link_state(s);
    // skip a terminator
    if (rhs_->check_reverse_transition(r, key_type::kTerminator))
        r = rhs_->prev(r);
    r = rhs_->go_backward(r, p, &mismatch);
    if (r == 1) {
        if (value)
            *value = index_[-lhs_->base(s)].data;
        return true;
    }
    return false;
}

void double_trie::build(const char *filename, bool verbose)
{
    FILE *out;

    if (!filename)
        throw std::runtime_error(std::string("can not save to file ")
                                 + filename);

    if ((out = fopen(filename, "w+"))) {
        header_->index_size = next_index_;
        header_->accept_size = next_accept_;
        fwrite(header_, sizeof(header_type), 1, out);
        fwrite(index_, sizeof(index_type) * header_->index_size, 1, out);
        fwrite(accept_, sizeof(accept_type) * header_->accept_size, 1, out);
        fwrite(lhs_->compact_header(),
               sizeof(basic_trie::header_type), 1, out);
        fwrite(lhs_->states(), sizeof(basic_trie::state_type)
                               * lhs_->compact_header()->size, 1, out);
        fwrite(rhs_->compact_header(),
               sizeof(basic_trie::header_type), 1, out);
        fwrite(rhs_->states(), sizeof(basic_trie::state_type)
                               * rhs_->compact_header()->size, 1, out);
        fclose(out);
        if (verbose) {
            char buf[256];
            size_t size[4];
            size[0] = sizeof(index_type) * header_->index_size;
            size[1] = sizeof(accept_type) * header_->accept_size;
            size[2] = sizeof(basic_trie::state_type)
                      * lhs_->compact_header()->size;
            size[3] = sizeof(basic_trie::state_type)
                      * rhs_->compact_header()->size;

            std::cerr << "index = "
                      << pretty_size(size[0], buf, sizeof(buf));
            std::cerr << ", accept = "
                      << pretty_size(size[1], buf, sizeof(buf));
            std::cerr << ", front = "
                      << pretty_size(size[2], buf, sizeof(buf));
            std::cerr << ", rear = "
                      << pretty_size(size[3], buf, sizeof(buf));
            std::cerr << ", total = "
                      << pretty_size(size[0] + size[1] + size[2] + size[3],
                                     buf, sizeof(buf))
                      << std::endl;
        }
    }
}

// ************************************************************************
// * Implementation of suffix trie                                        *
// ************************************************************************

single_trie::single_trie(size_t size)
    :trie_(NULL), suffix_(NULL), header_(NULL), next_suffix_(1),
     mmap_(NULL), mmap_size_(NULL)
{
    trie_ = new basic_trie(size);
    header_ = new header_type();
    memset(&common_, 0, sizeof(common_));
    resize_suffix(size?size:basic_trie::kDefaultStateSize);
    resize_common(kDefaultCommonSize);
}

single_trie::single_trie(const char *filename)
    :trie_(NULL), suffix_(NULL), header_(NULL), next_suffix_(1),
     mmap_(NULL), mmap_size_(NULL)
{
    struct stat sb;
    int fd, retval;

    if (!filename)
        throw std::runtime_error(std::string("can not load from file ")
                                 + filename);

    fd = open(filename, O_RDONLY);
    if (fd < 0)
        throw std::runtime_error(strerror(errno));
    if (fstat(fd, &sb) < 0)
        throw std::runtime_error(strerror(errno));

    mmap_ = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mmap_ == MAP_FAILED)
        throw std::runtime_error(strerror(errno));
    while (retval = close(fd), retval == -1 && errno == EINTR) {
        // exmpty
    }
    mmap_size_ = sb.st_size;

    void *start;
    start = header_ = reinterpret_cast<header_type *>(mmap_);
    if (strcmp(header_->magic, magic_))
        throw std::runtime_error("file corrupted");
    // load suffix
    suffix_ = reinterpret_cast<suffix_type *>(
              reinterpret_cast<header_type *>(start) + 1);
    // load trie
    start = suffix_ + header_->suffix_size;
    trie_ = new basic_trie(start,
                          reinterpret_cast<basic_trie::header_type *>(start)
                          + 1);
}


single_trie::~single_trie()
{
    if (!mmap_) {
        sanity_delete(header_);
        resize(suffix_, 0, 0);   // free suffix_
        resize(common_.data, 0, 0);  // free common_.data
    }
    sanity_delete(trie_);
}

void single_trie::insert_suffix(size_type s,
                                const char_type *inputs,
                                value_type value)
{
    trie_->set_base(s, -next_suffix_);
    const char_type *p = inputs;
    do {
        // +1 for value
        if (next_suffix_ + 1 >= header_->suffix_size)
            resize_suffix(next_suffix_ + 1);
        suffix_[next_suffix_++] = *p;
    } while (*p++ != key_type::kTerminator);
    suffix_[next_suffix_++] = value;
}

void single_trie::create_branch(size_type s,
                                const char_type *inputs,
                                value_type value)
{
    basic_trie::extremum_type extremum = {0, 0};
    size_type start = -trie_->base(s);

    // find common string
    const char_type *p = inputs;
    size_t i = 0;
    do {
        if (suffix_[start] != *p)
            break;
        if (i + 1 >= common_.size)
            resize_common(i + 1);
        common_.data[i++] = *p;
        if (*p > extremum.max || !extremum.max)
            extremum.max = *p;
        if (*p < extremum.min || !extremum.min)
            extremum.min = *p;
        ++start;
    } while (*p++ != key_type::kTerminator);
    common_.data[i] = 0;  // end common string

    // check if already exists by checking if the last common char is
    // terminator
    if (i > 0 && common_.data[i - 1] == key_type::kTerminator) {
        // duplicated key
        suffix_[start] = value;
        return;
    }

    // if there is a common part, insert common string into trie
    if (common_.data[0]) {
        trie_->set_base(s, trie_->find_base(common_.data, extremum));
        for (i = 0; common_.data[i]; i++)
            s = trie_->create_transition(s, common_.data[i]);
    } else {
       trie_->set_base(s, 0);
    }

    // create twig for old suffix
    size_type t = trie_->create_transition(s, suffix_[start]);
    trie_->set_base(t, -(start + 1));

    // create twig for new suffix
    t = trie_->create_transition(s, *p);
    if (*p == key_type::kTerminator) {
        trie_->set_base(t, -next_suffix_);
        suffix_[next_suffix_++] = value;
    } else {
        insert_suffix(t, p + 1, value);
    }
}


void single_trie::insert(const key_type &key, const value_type &value)
{
    const char_type *p;
    size_type s = trie_->go_forward(1, key.data(), &p);
    if (trie_->base(s) < 0) {
        if (p) {
            create_branch(s, p, value);
        } else {
            // duplicated key
            suffix_[-trie_->base(s)] = value;
        }
    } else {
        s = trie_->create_transition(s, *p);
        if (*p == key_type::kTerminator) {
            trie_->set_base(s, -next_suffix_);
            suffix_[next_suffix_++] = value;
        } else {
            insert_suffix(s, p + 1, value);
        }
    }
}

bool single_trie::search(const key_type &key, value_type *value) const
{
    const char_type *p;
    size_type s = trie_->go_forward(1, key.data(), &p);
    if (trie_->base(s) < 0) {
        size_type start = -trie_->base(s);
        if (p) {
            do {
                if (*p != suffix_[start++])
                    return false;
            } while (*p++ != key_type::kTerminator);
        }
        if (value)
            *value = suffix_[start];
        return true;
    }
    return false;
}

void single_trie::build(const char *filename, bool verbose)
{
    FILE *out;

    if (!filename)
        throw std::runtime_error(std::string("can not save to file ")
                                 + filename);

    if ((out = fopen(filename, "w+"))) {
        snprintf(header_->magic, sizeof(header_->magic), "%s", magic_);
        header_->suffix_size = next_suffix_ - 1;
        fwrite(header_, sizeof(header_type), 1, out);
        fwrite(suffix_, sizeof(suffix_type) * header_->suffix_size, 1, out);
        fwrite(trie_->compact_header(),
               sizeof(basic_trie::header_type), 1, out);
        fwrite(trie_->states(), sizeof(basic_trie::state_type)
                               * trie_->compact_header()->size, 1, out);

        fclose(out);
        if (verbose) {
            char buf[256];
            size_t size[2];
            size[0] = sizeof(suffix_type) * header_->suffix_size;
            size[1] = sizeof(basic_trie::state_type)
                      * trie_->compact_header()->size;

            std::cerr << "suffix = " << pretty_size(size[0], buf, sizeof(buf));
            std::cerr << ", trie = " << pretty_size(size[1], buf, sizeof(buf));
            std::cerr << ", total = "
                      << pretty_size(size[0] + size[1], buf, sizeof(buf))
                      << std::endl;
        }
    }
}

END_TRIE_NAMESPACE

// vim: ts=4 sw=4 ai et
