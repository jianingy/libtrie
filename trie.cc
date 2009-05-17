// Copyright Jianing Yang <detrox@gmail.com> 2009

#include <iostream>
#include <cassert>
#include "trie.h"

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
            // i += min
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
        // create new links according old ones
        set_base(nbase + inputs[i], base(obase + inputs[i]));
        set_check(nbase + inputs[i], check(obase + inputs[i]));
        find_exist_target(obase + inputs[i], targets, NULL);
        for (char_type *p = targets; *p; p++) {
            set_check(base(obase + inputs[i]) + *p, nbase + inputs[i]);
        }
        // if where we are standing is moving, we move with it
        if (stand == obase + inputs[i])
            stand = nbase + inputs[i];
        // free old places
        set_base(obase + inputs[i], 0);
        set_check(obase + inputs[i], 0);
    }
    // finally, set new base
    set_base(s, nbase);

    return stand;
}

basic_trie::size_type
basic_trie::create_link(size_type s, char_type ch)
{
    char_type targets[kCharsetSize + 1];
    char_type parent_targets[kCharsetSize + 1];
    extremum_type extremum = {0, 0}, parent_extremum = {0, 0};

    size_type t = next(s, ch);

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

    size_type s, t;
    const char *p;

    for (p = inputs, s = 1; p < inputs + length; p++) {
        char_type ch = char_in(*p);
        if (!(t = go_forward(s, ch))) {
            for (/* empty */; p < inputs + length; p++) {
                ch = char_in(*p);
                s = create_link(s, ch);
            }
            break;
        }
        s = t;
    }

	// add a terminator
	s = create_link(s, kTerminator);
    set_base(s, val);
}


basic_trie::value_type basic_trie::search(const char *inputs, size_t length)
{
    size_type s;
    const char *p;

    for (p = inputs, s = 1; p < inputs + length; p++) {
        char_type ch = char_in(*p);
        if (!(s = go_forward(s, ch)))
            return 0;
    }
	
	if (!(s = go_forward(s, kTerminator)))
		return 0;

    return s?base(s):0;
}

void basic_trie::trace(size_type s)
{
    size_type num_target;
    char_type targets[kCharsetSize + 1];

    trace_stack_.push_back(s);
    if ((num_target = find_exist_target(s, targets, NULL))) {
        for (char_type *p = targets; *p; p++)
            trace(go_forward(s, *p));
    } else {
        size_type cbase = 0, obase = 0;
        std::cerr << "transition => ";
        std::vector<size_type>::const_iterator it;
        for (it = trace_stack_.begin();it != trace_stack_.end(); it++) {
            cbase = base(*it);
            if (obase) {
				if (*it - obase == kTerminator) {
					std::cerr << "-'#'->";
				} else {
					char ch = char_out(*it - obase);
					if (isalnum(ch))
						std::cerr << "-'" << ch << "'->";
					else
						std::cerr << "-<" << std::hex << static_cast<int>(ch) << ">->";
				}
            }
            std::clog << *it << "[" << cbase << "]";
            obase = cbase;
        }
        std::cerr << "->{" << std::dec << (cbase) << "}" << std::endl;
    }
    trace_stack_.pop_back();
}
