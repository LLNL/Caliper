/*
This file is part of GOTCHA.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/gotcha/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "elf_ops.h"

#include <elf.h>

#include "libc_wrappers.h"
struct gnu_hash_header {
  uint32_t nbuckets;  //!< The number of buckets to hash symbols into
  uint32_t symndx;    //!< Index of the first symbol accessible via hashtable in
                      //!< the symbol table
  uint32_t maskwords;  //!< Number of words in the hash table's bloom filter
  uint32_t shift2;     //!< The bloom filter's shift count
};

static uint32_t gnu_hash_func(const char *str) {
  uint32_t hash = 5381;
  for (; *str != '\0'; str++) {
    hash = hash * 33 + *str;
  }
  return hash;
}

/* Symbol versioning
 *
 * https://refspecs.linuxfoundation.org/LSB_3.0.0/LSB-PDA/LSB-PDA.junk/symversion.html
 *
 * versym[symidx] does only provides an index into the ElfW(Verdef) array
 * (DT_VERDEF/SHT_GNU_verdef) and is not the version itself, but SHT_GNU_verdef
 * is sorted in ascending order and the entries have a parent relation, thus a
 * higher index should always be a higher version. As we only search for the
 * latest symbol/highest version it is sufficient to compare the index.
 */

signed long lookup_gnu_hash_symbol(const char *name, ElfW(Sym) * syms,
                                   ElfW(Half) * versym, char *symnames,
                                   void *sheader) {
  uint32_t *buckets = NULL, *vals = NULL;
  uint32_t hash_val = 0;
  uint32_t cur_sym = 0, cur_sym_hashval = 0;
  signed long latest_sym = -1;
  ElfW(Half) latest_sym_ver = 0;
  struct gnu_hash_header *header = (struct gnu_hash_header *)(sheader);

  buckets = (uint32_t *)(((unsigned char *)(header + 1)) +
                         (header->maskwords * sizeof(ElfW(Addr))));
  vals = buckets + header->nbuckets;

  hash_val = gnu_hash_func(name);
  cur_sym = buckets[hash_val % header->nbuckets];
  if (cur_sym == 0) {
    return -1;
  }

  hash_val &= ~1;
  for (;;) {
    cur_sym_hashval = vals[cur_sym - header->symndx];
    if (((cur_sym_hashval & ~1) == hash_val) &&
        (gotcha_strcmp(name, symnames + syms[cur_sym].st_name) == 0)) {
      if (!versym) {
        return (signed long)cur_sym;
      }

      if ((versym[cur_sym] & 0x7fff) > latest_sym_ver) {
        latest_sym = (signed long)cur_sym;
        latest_sym_ver = versym[cur_sym] & 0x7fff;
      }
    }
    if (cur_sym_hashval & 1) {
      break;
    }
    cur_sym++;
  }

  return latest_sym;
}

static unsigned long elf_hash(const unsigned char *name) {
  unsigned int h = 0, g = 0;
  while (*name != '\0') {
    h = (h << 4) + *name++;
    if ((g = h & 0xf0000000)) {
      h ^= g >> 24;
    }
    h &= ~g;
  }
  return h;
}

signed long lookup_elf_hash_symbol(const char *name, ElfW(Sym) * syms,
                                   ElfW(Half) * versym, char *symnames,
                                   ElfW(Word) * header) {
  ElfW(Word) *nbucket = header + 0;
  ElfW(Word) *buckets = header + 2;
  ElfW(Word) *chains = buckets + *nbucket;
  signed long latest_sym = -1;
  ElfW(Half) latest_sym_ver = 0;

  unsigned int x = elf_hash((const unsigned char *)name);
  signed long y = (signed long)buckets[x % *nbucket];
  while (y != STN_UNDEF) {
    if (gotcha_strcmp(name, symnames + syms[y].st_name) == 0) {
      if (!versym) {
        // in general all libs would have version but it is a guard condition.
        return y;  // GCOVR_EXCL_LINE
      }

      if ((versym[y] & 0x7fff) > latest_sym_ver) {
        latest_sym = y;
        latest_sym_ver = versym[y] & 0x7fff;
      }
    }
    y = chains[y];
  }

  return latest_sym;
}
