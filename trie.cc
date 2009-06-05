// Copyright Jianing Yang <detrox@gmail.com> 2009

#include <iostream>
#include <cassert>
#include "trie.h"

// ************************************************************************
// * Implementation of basic_trie                                         *
// ************************************************************************

basic_trie::basic_trie(size_type size, 
                       trie_relocator_interface<size_type> *relocator)
    :header_(NULL), states_(NULL), last_base_(0), owner_(true),
     relocator_(relocator)
{
    if (size < kCharsetSize)
        size = kCharsetSize;
    header_ = static_cast<header_type *>(malloc(sizeof(header_type)));
    if (!header_)
        throw std::bad_alloc();
    memset(header_, 0, sizeof(header_type));
    inflate(size);
}

basic_trie::basic_trie(void *header, void *states)
    :header_(NULL), states_(NULL), last_base_(0), owner_(false),
     relocator_(NULL)
{
    header_ = static_cast<header_type *>(header);
    states_ = static_cast<state_type *>(states);
}

basic_trie::basic_trie(const basic_trie &trie)
    :header_(NULL), states_(NULL), last_base_(0), owner_(false),
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
            free(header_);
            header_ = NULL;
        }
        if (states_) {
            free(states_);
            states_ = NULL;  // set to NULL for next realloc
        }
    }
    owner_ = true;
    header_ = static_cast<header_type *>(malloc(sizeof(header_type)));
    if (!header_)
        throw std::bad_alloc();
    states_ = static_cast<state_type *>
              (malloc(trie.header()->size * sizeof(state_type)));
    if (!states_)
        throw std::bad_alloc();
    memcpy(header_, trie.header(), sizeof(header_type));
    memcpy(states_, trie.states(), trie.header()->size * sizeof(state_type));
}

basic_trie::~basic_trie()
{
    if (owner_) {
        if (header_) {
            free(header_);
            header_ = NULL;
        }
        if (states_) {
            free(states_);
            states_ = NULL;
        }
    }
}

basic_trie::size_type
basic_trie::find_base(const char_type *inputs, const extremum_type &extremum)
{
    bool found;
    size_type i;
    const char_type *p;

    for (i = last_base_, found = false; !found; /* empty */) {
        i++;
        if (i + extremum.max >= header_->size)
            inflate(extremum.max);
        if (check(i + extremum.min) <= 0 && check(i + extremum.max) <= 0) {
            for (p = inputs, found = true; *p; p++) {
                if (check(i + *p) > 0) {
                    found = false;
                    break;
                }
            }
        } else {
            //i += extremum.min;
        }
    }

    last_base_ = i;

    return last_base_;
}

basic_trie::size_type
basic_trie::relocate(size_type stand,
                     size_type s,
                     const char_type *inputs,
                     const extremum_type &extremum)
{
    size_type obase, nbase, i;
    char_type targets[kCharsetSize + 1];

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

basic_trie::size_type
basic_trie::create_transition(size_type s, char_type ch)
{
    char_type targets[kCharsetSize + 1];
    char_type parent_targets[kCharsetSize + 1];
    extremum_type extremum = {0, 0}, parent_extremum = {0, 0};

    size_type t = next(s, ch);
    if (t >= header_->size)
        inflate(t - header_->size + 1);

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
            inflate(t - header_->size + 1);
    }
    set_check(t, s);

    return t;
}


void basic_trie::insert(const char *inputs, size_t length, value_type val)
{
    if (val < 1)
        throw std::runtime_error("basic_trie::insert: value must > 0");

    if (!inputs)
        throw std::runtime_error("basic_trie::insert: input pointer is null");

    size_type s;
    const char *p;

    s = go_forward(1, inputs, length, &p);

    for (/* empty */; p < inputs + length; p++) {
        char_type ch = char_in(*p);
        s = create_transition(s, ch);
    }

    // add a terminator
    s = create_transition(s, kTerminator);
    set_base(s, val);
}


basic_trie::value_type
basic_trie::search(const char *inputs, size_t length) const
{
    size_type s = go_forward(1, inputs, length, NULL);
    size_type t = next(s, kTerminator);

    if (!check_transition(s, t))
        return 0;

    return base(t);
}

void basic_trie::trace(size_type s) const
{
    size_type num_target;
    char_type targets[kCharsetSize + 1];
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
                if (*it - obase == kTerminator) {
                    std::cerr << "-#->";
                } else {
                    char ch = char_out(*it - obase);
                    if (isalnum(ch))
                        std::cerr << "-'" << ch << "'->";
                    else
                        std::cerr << "-<" << std::hex
                                  << static_cast<int>(ch) << ">->";
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
// * Implementation of trie                                               *
// ************************************************************************

trie::trie()
    :lhs_(NULL), rhs_(NULL), index_(NULL), accept_(NULL),
     index_size_(0), accept_size_(0), next_accept_(1), next_index_(1),
     front_relocator_(NULL), rear_relocator_(NULL), stand_(0)
{
    front_relocator_ = new trie_relocator<trie>(this, &trie::relocate_front);
    rear_relocator_ = new trie_relocator<trie>(this, &trie::relocate_rear);
    lhs_ = new basic_trie();
    rhs_ = new basic_trie();
    lhs_->set_relocator(front_relocator_);
    rhs_->set_relocator(rear_relocator_);
}

trie::~trie()
{
    if (lhs_) {
        delete lhs_;
        lhs_ = NULL;
    }
    if (rhs_) {
        delete rhs_;
        rhs_ = NULL;
    }
    if (index_) {
        free(index_);
        index_ = NULL;
        index_size_ = 0;
    }
    if (accept_) {
        free(accept_);
        accept_ = NULL;
        accept_size_ = 0;
    }
    if (front_relocator_) {
        delete front_relocator_;
        front_relocator_ = NULL;
    }
    if (rear_relocator_) {
        delete rear_relocator_;
        rear_relocator_ = NULL;
    }
}

basic_trie::size_type
trie::rhs_append(const char *inputs, size_t length)
{
    const char *p = NULL;
    size_type s = 1, t;

    t = rhs_->next(s, basic_trie::kTerminator);
    if (rhs_->check_transition(s, t)) {
        s = rhs_->go_forward_reverse(t, inputs, length, &p);
        if (p < inputs) {  // all characters match
            t = rhs_->next(s, basic_trie::kTerminator);
            if (outdegree(s) == 0)
                return s;
            else if (rhs_->check_transition(s, t))
                return rhs_->next(s, basic_trie::kTerminator);
            else
                return rhs_->create_transition(s, basic_trie::kTerminator);
        }
    }
    if (outdegree(s) == 0) {
        t = rhs_->create_transition(s, basic_trie::kTerminator);
        std::set<size_type>::const_iterator it;
        for (it = refer_[s].referer.begin();
                it != refer_[s].referer.end();
                it++) {
            set_link(*it, t);
        }
        refer_.erase(s);
    }
    if (s == 1) {
        p = inputs + length - 1;
        s = rhs_->create_transition(s, basic_trie::kTerminator);
    }
    for (; p >= inputs; p--) {
        t = rhs_->create_transition(s, basic_trie::char_in(*p));
        s = t;
    }
    return s;
}

void trie::lhs_insert(size_type s, const char *inputs, size_t length)
{
    size_type t = lhs_->create_transition(s, basic_trie::char_in(inputs[0]));
    set_link(t, rhs_append(inputs + 1, length - 1));
}

void trie::rhs_clean_more(size_type t)
{
    assert(t > 0);
    if (outdegree(t) == 0 && count_referer(t) == 0) {
        size_type s = rhs_->prev(t);
        remove_accept_state(t);
        if (s > 0)
            rhs_clean_more(s);
    } else if (outdegree(t) == 1) {
        size_type r = rhs_->next(t, basic_trie::kTerminator);
        if (rhs_->check_transition(t, r)) {
            // delete transition 't -#-> r'
            std::set<size_type>::const_iterator it;
            for (it = refer_[r].referer.begin();
                    it != refer_[r].referer.end();
                    it++) 
                set_link(*it, t);
//            if (refer_.find(t) != refer_.end())
//                assert (accept_[refer_[t].accept_index].accept == r || accept_[refer_[t].accept_index].accept == t);
            accept_[refer_[t].accept_index].accept = t;
            remove_accept_state(r);
        }
    }
}

void trie::rhs_insert(size_type s, size_type r, 
                      const char *match, size_t match_length,
                      const char *remain, size_t remain_length,
                      char ch, bool terminator)
{
    // R-1
    size_type u = link_state(s); // u might be zero
    index_[-lhs_->base(s)].index = 0;
    lhs_->set_base(s, 0); // XXX: Bug?
    stand_ = r;
    if (u > 0) {
        refer_[u].referer.erase(s);

        if (refer_[u].referer.size() == 0)
            refer_.erase(u);
    }

    // R-2
    const char *p;
    size_type t;
    //printf("match_length = %d\n", match_length);
    for (p = match; p < match + match_length; p++) {
        t = lhs_->create_transition(s, basic_trie::char_in(*p));
        s = t;
    }
    t = lhs_->create_transition(s, basic_trie::char_in(*remain));
    set_link(t, rhs_append(remain + 1, remain_length - 1));

    // R-3
    t = lhs_->create_transition(s, (terminator)?basic_trie::kTerminator:
                                                basic_trie::char_in(ch));
    size_type v = rhs_->prev(stand_); // v -ch-> r
    if (!rhs_->check_transition(v, rhs_->next(v, basic_trie::kTerminator)))
        r = rhs_->create_transition(v, basic_trie::kTerminator);
    else
        r = rhs_->next(v, basic_trie::kTerminator);

    set_link(t, r);

    // R-4
    if (u > 0) {
        if (!rhs_clean_one(u))
            rhs_clean_more(u);
    }

}

void trie::insert(const char *inputs, size_t length)
{
    size_type s;
    const char *p;
    s = lhs_->go_forward(1, inputs, length, &p);
    if (p < inputs + length && !check_separator(s)) {
        lhs_insert(s, p, length - (p - inputs));
        return;
    }
    if (p >= inputs + length)
        return;

    size_type r = link_state(s);
    if (rhs_->check_reverse_transition(r, basic_trie::kTerminator))
        r = rhs_->prev(r);

    char last;
    bool terminator = false;
    exists_.clear();
    for (; p < inputs + length; p++) {
        if (rhs_->check_reverse_transition(r, basic_trie::char_in(*p))) {
            r = rhs_->prev(r);
            exists_.append(1, *p);
        } else {
            last = basic_trie::char_out(r - rhs_->base(rhs_->prev(r)));
            terminator = (r - rhs_->base(rhs_->prev(r)) == basic_trie::kTerminator)?true:false;
            break;
        }
    }
    if (p < inputs + length)
        rhs_insert(s, r, exists_.c_str(), exists_.length(),
                   p, length - (p - inputs), last, terminator);
    return;
}

int trie::search(const char *inputs, size_t length)
{
    size_type s;
    const char *p;
    s = lhs_->go_forward(1, inputs, length, &p);
    if (p < inputs + length && !check_separator(s))
        return -1;
    if (p >= inputs + length)
        return true;
    size_type r = link_state(s);
    if (rhs_->check_reverse_transition(r, basic_trie::kTerminator))
        r = rhs_->prev(r);

    r = rhs_->go_backward(r, p, length - (p - inputs), NULL);
    r = rhs_->prev(r);    
    if (r == 1)
        return true;
    return -2;
}

// vim: ts=4 sw=4 ai et
