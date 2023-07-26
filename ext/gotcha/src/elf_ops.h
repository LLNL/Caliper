/*
This file is part of GOTCHA.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/gotcha/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
and conditions of the GNU Lesser General Public License for more details.  You should
have received a copy of the GNU Lesser General Public License along with this
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#if !defined(ELF_OPS_H_)
#define ELF_OPS_H_

#include <elf.h>
#include <link.h>
#include "library_filters.h"


/*!
 ******************************************************************************
 * \def GOTCHA_CHECK_VISIBILITY(sym)
 * \brief Checks whether a given symbol is associated with a real function
 * \param sym The symbol you wish to check
 ******************************************************************************
 */
#define GOTCHA_CHECK_VISIBILITY(sym)((sym.st_size>0))

/*!
 ******************************************************************************
 *
 * \fn signed long lookup_gnu_hash_symbol(const char *name, 
                                          ElfW(Sym) *syms,
                                          ElfW(Half) *versym,
                                          char *symnames, 
                                          void *header);
 *
 * \brief Looks up the index of a symbol in a symbol table given a symbol name 
 *
 * \param name     The name of the function to be looked up
 * \param syms     The pointer to the symbol table
 * \param versym   The pointer to the symbol versioning table
 * \param symnames A pointer into the string table
 * \param header   The parameters the underlying GNU Hash function will use
 *
 ******************************************************************************
 */
signed long lookup_gnu_hash_symbol(const char *name, ElfW(Sym) *syms, ElfW(Half) *versym, char *symnames, void *sheader);

/*!
 ******************************************************************************
 *
 * \fn signed long lookup_elf_hash_symbol(const char *name, 
                                          ElfW(Sym) *syms,
                                          ElfW(Half) *versym,
                                          char *symnames, 
                                          ElfW(Word) *header);
 *
 * \brief Looks up the index of a symbol in a symbol table given a symbol name 
 *
 * \param name     The name of the function to be looked up
 * \param syms     The pointer to the symbol table
 * \param versym   The pointer to the symbol versioning table
 * \param symnames A pointer into the string table
 * \param header   The parameters the underlying ELF Hash function will use
 *
 ******************************************************************************
 */
signed long lookup_elf_hash_symbol(const char *name, ElfW(Sym) *syms, ElfW(Half) *versym, char *symnames, ElfW(Word) *header);

/*!
 ******************************************************************************
 *
 * \def INIT_DYNAMIC(lmap)
 *
 * \brief This macro initializes a set of variables from an link.h
 *        link_map entry
 *
 * \param lmap         The link map entry associated with this symbol
 * \param[out] dynsec  The dynamic section associated with the link_map
 * \param[out] rela
 * \param[out] rel
 * \param[out] jmprel
 * \param[out] symtab
 * \param[out] gnu_hash
 * \param[out] got
 * \param[out] strtab
 * \param[out] versym
 *
 ******************************************************************************
 */
#define INIT_DYNAMIC(lmap)                                      \
   ElfW(Dyn) *dynsec = NULL, *dentry = NULL;                    \
   ElfW(Rela) *rela = NULL;                                     \
   ElfW(Rel) *rel = NULL;                                       \
   ElfW(Addr) jmprel = 0;                                       \
   ElfW(Sym) *symtab = NULL;                                    \
   ElfW(Addr) gnu_hash = 0x0, elf_hash = 0x0;                   \
   ElfW(Addr) got = 0x0;                                        \
   ElfW(Half) *versym = NULL;                                   \
   char *strtab = NULL;                                         \
   unsigned int rel_size = 0, rel_count = 0, is_rela = 0, i;    \
   dynsec = lmap->l_ld;                                         \
   if (!dynsec)                                                 \
      return -1;                                                \
   for (dentry = dynsec; dentry->d_tag != DT_NULL; dentry++) {  \
      switch (dentry->d_tag) {                                  \
         case DT_REL: {                                         \
            rel = (ElfW(Rel) *)dentry->d_un.d_ptr;                           \
            break;                                              \
         }                                                      \
         case DT_RELA: {                                         \
            rela = (ElfW(Rela) *) dentry->d_un.d_ptr;                           \
            break;                                              \
         }                                                      \
         case DT_PLTRELSZ: {                                    \
            rel_size = (unsigned int) dentry->d_un.d_val;       \
            break;                                              \
         }                                                      \
         case DT_PLTGOT: {                                      \
            got = dentry->d_un.d_ptr;                           \
            break;                                              \
         }                                                      \
         case DT_HASH: {                                        \
            elf_hash = dentry->d_un.d_val;                      \
            break;                                              \
         }                                                      \
         case DT_STRTAB: {                                      \
            strtab = (char *) dentry->d_un.d_ptr;               \
            break;                                              \
         }                                                      \
         case DT_SYMTAB: {                                      \
            symtab = (ElfW(Sym) *) dentry->d_un.d_ptr;          \
            break;                                              \
         }                                                      \
         case DT_PLTREL: {                                      \
            is_rela = (dentry->d_un.d_val == DT_RELA);          \
            break;                                              \
         }                                                      \
         case DT_JMPREL: {                                      \
            jmprel = dentry->d_un.d_val;                        \
            break;                                              \
         }                                                      \
         case DT_GNU_HASH: {                                    \
            gnu_hash = dentry->d_un.d_val;                      \
            break;                                              \
         }                                                      \
         case DT_VERSYM: {                                      \
            versym = (ElfW(Half) *) dentry->d_un.d_ptr;         \
            break;                                              \
         }                                                      \
      }                                                         \
   }                                                            \
   rel_count = rel_size / (is_rela ? sizeof(ElfW(Rela)) : sizeof(ElfW(Rel))); \
   (void) rela;                                                 \
   (void) rel;                                                  \
   (void) jmprel;                                               \
   (void) symtab;                                               \
   (void) gnu_hash;                                             \
   (void) elf_hash;                                             \
   (void) got;                                                  \
   (void) strtab;                                               \
   (void) rel_size;                                             \
   (void) rel_count;                                            \
   (void) is_rela;                                              \
   (void) versym;                                               \
   (void) i;

/*!
 ******************************************************************************
 *
 * \def FOR_EACH_PLTREL_INT(relptr, op, ...)
 *
 * \brief This macro calls an operation on each relocation in a dynamic section.
 *        It should be called only by FOR_EACH_PLTREL, users should not call it
 *
 * \param relptr       The pointer to the first relocation in the section
 * \param op           The operation to be performed on each relocation
 * \param ...          Any additional arguments you wish to pass to op
 *
 ******************************************************************************
 */
#define FOR_EACH_PLTREL_INT(relptr, op, ...)                        \
   for (i = 0; i < rel_count; i++) {                           \
      ElfW(Addr) offset = relptr[i].r_offset;                  \
      unsigned long symidx = R_SYM(relptr[i].r_info);          \
      ElfW(Sym) *sym = symtab + symidx;                        \
      char *symname = strtab + sym->st_name;                   \
      op(sym, symname, offset, ## __VA_ARGS__);                                \
   }

/*!
 ******************************************************************************
 *
 * \def FOR_EACH_PLTREL(lmap, op, ...)
 *
 * \brief This macro calls an operation on each relocation in the dynamic section
 *        associated with a link map entry
 *
 * \param lmap         The link map whose relocations you want processed
 * \param op           The operation to be performed on each relocation
 * \param ...          Any additional arguments you wish to pass to op
 *
 ******************************************************************************
 */
#define FOR_EACH_PLTREL(lookup_rel, lmap, op, ...) {                            \
      INIT_DYNAMIC(lmap)                                       \
      ElfW(Addr) offset = lmap->l_addr;                        \
      (void) offset;                                           \
      if (is_rela) {                                           \
         ElfW(Rela) * jmp_rela = (ElfW(Rela) *) jmprel;                         \
         FOR_EACH_PLTREL_INT(jmp_rela, op, ## __VA_ARGS__);         \
         if (lookup_rel && rela) {                                                 \
            FOR_EACH_PLTREL_INT(rela, op, ## __VA_ARGS__);             \
         }                                                             \
      }                                                        \
      else {                                                   \
         ElfW(Rel) * jmp_rel = (ElfW(Rel) *) jmprel;                           \
         FOR_EACH_PLTREL_INT(jmp_rel, op, ## __VA_ARGS__);          \
         if (lookup_rel && rel) {                                                 \
             FOR_EACH_PLTREL_INT(rel, op, ## __VA_ARGS__);                                                       \
         }                                                    \
      }                                                        \
   }


#endif
