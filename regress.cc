// Copyright Jianing Yang <jianingy.yang@gmail.com> 2009

#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <math.h>
#include "trie.h"


#define length(x) (strlen(x))
//#define value(x, y) (int)(7919/(x * x + y + 1))
#define value(x, y) (int)(j + 1)



int main()
{
	size_t i, j;
	int val;
	const char *dict[][256] = {
		{"baby", "bachelor", "back", "badge", "badger", "badness", "bcs"},
		{"in", "inspiration", "instant", "instrument", "prevision", "precession", "procession", "provision", NULL},
		{"moldy","molochize","Molochize","molochized","Molochize's","monarchize", NULL},
		{"a", "abilities", "ability's", "about", "absence", "absence's", "absolute", "absolutely", "academic", "acceptable", NULL},
		{"sepaled", "Septembrizers", "septemia", "septicemia", "septicemias", NULL},
		{"abcd", "zdd", NULL},
		//{"bcd", "bc", "b", "bc", "bcd"},
		//{"a", "ab", "abc", "bcd", "bc", "b"},
		//{"a", "ab", "abc", "ab", "a"},
		{NULL}
	};

	printf("libxtree regress testing (exclude load and mmap)\n");
	printf("================================================\n");

/* basic_trie */
#if 1
	printf("\nbasic_trie\n");
	printf("----------\n");
	for (i = 0; dict[i][0]; i++) {
		basic_trie btrie;
		printf("wordset %d: ", i);
		// test for duplicated keys
//		for (j = 0; dict[i][j]; j++)
//			btrie.insert(dict[i][j], length(dict[i][j]), j + 1);
		for (j = 0; dict[i][j]; j++)
			btrie.insert(dict[i][j], length(dict[i][j]), value(j, i));
		for (j = 0; dict[i][j]; j++) {
			if (btrie.search(dict[i][j], length(dict[i][j]), &val) && val == value(j, i)) {
				printf("[%d] ", val);
			} else {
				printf("\nTEST FAILED on '%s'!\n", dict[i][j]);
				btrie.trace(1);
				exit(0);
			}
		}
		printf("\n");
	}

	printf("\nbasic_trie copy constructor\n");
	printf("----------\n");
	for (i = 0; dict[i][0]; i++) {
		basic_trie btrie;
		printf("wordset %d: ", i);
		for (j = 0; dict[i][j]; j++)
			btrie.insert(dict[i][j], length(dict[i][j]), value(j, i));
		basic_trie ctrie(btrie);
		for (j = 0; dict[i][j]; j++) {
			if (ctrie.search(dict[i][j], length(dict[i][j]), &val) && val == value(j, i)) {
				printf("[%d] ", val);
			} else {
				printf("\nTEST FAILED on '%s'!\n", dict[i][j]);
				ctrie.trace(1);
				exit(0);
			}
		}
		printf("\n");
	}

	printf("\nbasic_trie operator = \n");
	printf("----------\n");
	for (i = 0; dict[i][0]; i++) {
		basic_trie btrie;
		printf("wordset %d: ", i);
		for (j = 0; dict[i][j]; j++)
			btrie.insert(dict[i][j], length(dict[i][j]), value(j, i));
		basic_trie ctrie;
		ctrie = btrie;
		for (j = 0; dict[i][j]; j++) {
			if (ctrie.search(dict[i][j], length(dict[i][j]), &val) && val == value(j, i)) {
				printf("[%d] ", val);
			} else {
				printf("\nTEST FAILED on '%s'!\n", dict[i][j]);
				ctrie.trace(1);
				exit(0);
			}
		}
		printf("\n");
	}
#endif

/* double_trie */
	printf("\ndouble_trie\n");
	printf("----------\n");
	for (i = 0; dict[i][0]; i++) {
		double_trie btrie;
		printf("wordset %d: ", i);
		for (j = 0; dict[i][j]; j++)
			btrie.insert(dict[i][j], length(dict[i][j]), -value(j, i) + 3);
		for (j = 0; dict[i][j]; j++) {
			if (btrie.search(dict[i][j], length(dict[i][j]), &val) && val == -value(j, i) + 3) {
				printf("[%d] ", val);
			} else {
				printf("\nTEST FAILED on '%s' = %d!\n", dict[i][j], val);
				btrie.trace_table(0, 0);
				std::cout << "FRONT: \n";
				btrie.front_trie()->trace(1);
				std::cout << "REAR: \n";
				btrie.rear_trie()->trace(1);
				exit(0);
			}
		}
		printf("\n");
	}

/* binary test */
	do {
		double_trie btrie;
		bool fail = false;
		printf("\ndouble_trie binary data\n");
		printf("-------------------------\nset 0:");
		btrie.insert("\x00\x01\x02", 3, 1);
		btrie.insert("\x02\x00\x01", 3, 2);
		btrie.insert("\x02\x01\x00", 3, 3);
		if (!btrie.search("\x00\x01\x02", 3, &val))
			fail = true;
		printf("%d ", val);
		if (!btrie.search("\x02\x00\x01", 3, &val))
			fail = true;
		printf("%d ", val);
		if (!btrie.search("\x02\x01\x00", 3, &val))
			fail = true;
		printf("%d\n", val);
		if (fail) {
			printf("\nTEST FAILED on '%s'!\n", dict[i][j]);
			btrie.trace_table(0, 0);
			std::cout << "FRONT: \n";
			btrie.front_trie()->trace(1);
			std::cout << "REAR: \n";
			btrie.rear_trie()->trace(1);
			exit(0);
		}
	} while (0);

/* single_trie */
	printf("\nsingle_trie\n");
	printf("----------\n");
	for (i = 0; dict[i][0]; i++) {
		single_trie btrie;
		printf("wordset %d: ", i);
		for (j = 0; dict[i][j]; j++)
			btrie.insert(dict[i][j], length(dict[i][j]), -value(j, i) + 1);
		for (j = 0; dict[i][j]; j++) {
			if (btrie.search(dict[i][j], length(dict[i][j]), &val) && val == -value(j, i) + 1) {
				printf("[%d] ", val);
			} else {
				printf("\nTEST FAILED on '%s'!\n", dict[i][j]);
				std::cout << "TRIE: \n";
				btrie.trie()->trace(1);
				btrie.trace_suffix(0, 100);
				exit(0);
			}
		}
		printf("\n");
	}
}
