AM_CPPFLAGS=-I$(srcdir)/../include
lib_LTLIBRARIES=libtrie.la
libtrie_la_SOURCES=trie_impl.h trie_impl.cc $(srcdir)/../include/trie.h trie.cc
bin_PROGRAMS = trietool
trietool_SOURCES = trie_tool.cc
trietool_LDADD = libtrie.la
include_HEADERS = $(srcdir)/../include/trie.h
