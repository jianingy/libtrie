/* bug found my Naoki Yoshinaga */

#include <cassert>

#include <iostream>
#include <trie_impl.h>

using namespace dutil;

int main ()
{
    dutil::trie* single_trie
            = dutil::trie::create_trie (dutil::trie::SINGLE_TRIE);
    single_trie->insert ("OK", 2, 1);
    single_trie->insert ("Jan", 3, 2);
    int val = 0;
    assert (! single_trie->search ("On", 2, &val));
    delete single_trie;
    dutil::trie* double_trie
            = dutil::trie::create_trie (dutil::trie::DOUBLE_TRIE);
    double_trie->insert ("OK", 2, 1);
    double_trie->insert ("Jan", 3, 2);
    val = 0;

dynamic_cast<dutil::double_trie *>(double_trie)->trace_table(0, 0);
    std::cout << "FRONT: \n";
dynamic_cast<dutil::double_trie *>(double_trie)->front_trie()->trace(1);
    std::cout << "REAR: \n";
dynamic_cast<dutil::double_trie *>(double_trie)->rear_trie()->trace(1);

    assert (! double_trie->search ("On", 2, &val));
    delete double_trie;
}
