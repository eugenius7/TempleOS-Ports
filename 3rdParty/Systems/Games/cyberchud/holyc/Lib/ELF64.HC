#define EI_NIDENT 16
#define EM_X86_64 0x3E
#define ET_EXEC 2
#define ET_DYN 3

#define STT_OBJECT 1
#define STT_FUNC 2

#define SHT_NOBITS 8

class Elf64_Ehdr {
  U8 e_ident[EI_NIDENT]; /* Magic number and other info */
  U16 e_type;            /* Object file type */
  U16 e_machine;         /* Architecture */
  U32 e_version;         /* Object file version */
  U64 e_entry;           /* Entry point virtual address */
  U64 e_phoff;           /* Program header table file offset */
  U64 e_shoff;           /* Section header table file offset */
  U32 e_flags;           /* Processor-specific flags */
  U16 e_ehsize;          /* ELF header size in bytes */
  U16 e_phentsize;       /* Program header table entry size */
  U16 e_phnum;           /* Program header table entry count */
  U16 e_shentsize;       /* Section header table entry size */
  U16 e_shnum;           /* Section header table entry count */
  U16 e_shstrndx;        /* Section header string table index */
};

class Elf64_Shdr {
  U32 sh_name;      /* Section name (string tbl index) */
  U32 sh_type;      /* Section type */
  U64 sh_flags;     /* Section flags */
  U64 sh_addr;      /* Section virtual addr at execution */
  U64 sh_offset;    /* Section file offset */
  U64 sh_size;      /* Section size in bytes */
  U32 sh_link;      /* Link to another section */
  U32 sh_info;      /* Additional section information */
  U64 sh_addralign; /* Section alignment */
  U64 sh_entsize;   /* Entry size if section holds table */
};

class Elf64_Sym {
  U32 st_name;  /* Symbol name (string tbl index) */
  U8 st_info;   /* Symbol type and binding */
  U8 st_other;  /* Symbol visibility */
  U16 st_shndx; /* Section index */
  U64 st_value; /* Symbol value */
  U64 st_size;  /* Symbol size */
};

class PLT_entry {
  U8 pad[0x10];
};

class RELA_entry {
  U64 r_offset;
  U64 r_info;
  I64 r_addend;
};

class Elf {
  union {
    U8 *u8;
    Elf64_Ehdr *ehdr;
  } I64 size;
  U8 *dynstr;
  Elf64_Sym *dynsym;
  PLT_entry *plt;
  RELA_entry *rela_dyn;
  RELA_entry *rela_plt;
  Elf64_Sym *strtab;
  Elf64_Sym *symtab;
  I64 rela_dyn_size;
  I64 rela_plt_size;
  I64 strtab_size;
  I64 symtab_size;
  I64 dynsym_size;
  U8 *segments;
};

/* Harness */
U0 (*_init)();
U0 (*_loop)();
I32 (*_check_if_quitting)();
U0 (*_quit)();


U0 unimplemented_symbol() {
  I32 s = 0xDEADF00D;
  "Unimplemented symbol: %s\n", s;
  while (1)
    Sleep(1);
}

U0 symbol_split(U8* entry_name, U8* entry_split) {
  I64 i, strlen = StrLen(entry_name);
  for (i=0; i<strlen; i++) {
    if (entry_name[i] == '@') {
      entry_split[i] = '\0';
      break;
    }
    entry_split[i] = entry_name[i];
  }
  entry_split[strlen] = '\0';
}

Bool is_valid_elf(Elf *elf) {
  Bool res = TRUE;
  if (MemCmp(elf->u8 + 1, "ELF", 3)) {
    CommPutS("Invalid signature (not ELF)\n");
    res = FALSE;
  }
  if (elf->ehdr->e_type != ET_EXEC && elf->ehdr->e_type != ET_DYN) {
    CommPutS("Invalid object file type\n");
    res = FALSE;
  }
  if (elf->ehdr->e_machine != EM_X86_64) {
    CommPutS("Invalid architecture\n");
    res = FALSE;
  }
  return res;
}

U64 get_symbol_address(U8 *entry_name) {
  I64 i;
  CHashSrcSym *sym;
  CHashTable *tbl = Fs->hash_table;
  while (tbl) {
    for (i = 0; i < tbl->mask; i++) {
      sym = tbl->body[i];
      while (sym) {
        if (sym->type == HTT_GLBL_VAR)
          if (!StrCmp(sym->str, entry_name))
            return sym(CHashGlblVar *)->data_addr;
        if (sym->type == HTT_FUN)
          if (!StrCmp(sym->str, entry_name))
            return sym(CHashFun *)->exe_addr;
        sym = sym->next;
      }
    }
    tbl = tbl->next;
  }
  return NULL;
}

CHashClass *get_symbol_hash_entry(U8 *entry_name) {
  I64 i;
  CHashSrcSym *sym;
  CHashTable *tbl = Fs->hash_table;
  while (tbl) {
    for (i = 0; i < tbl->mask; i++) {
      sym = tbl->body[i];
      while (sym) {
        if (sym->type == HTT_CLASS)
          if (!StrCmp(sym->str, entry_name))
            return sym;
        sym = sym->next;
      }
    }
    tbl = tbl->next;
  }
  return NULL;
}

U0 process_elf_section_header_table(Elf *elf) {
  Elf64_Shdr *shdr = elf->u8 + elf->ehdr->e_shoff;
  Elf64_Shdr *shdr_shstrtab = shdr + elf->ehdr->e_shstrndx;
  U8 *shstrtab = elf->u8 + shdr_shstrtab->sh_offset;

  I64 i = 0;
  U64 lastAddr = 0;
  U64 size = 0;
  for (i=0; i<elf->ehdr->e_shnum; i++, shdr++) {
    if (shdr->sh_addr >= lastAddr){
      lastAddr = shdr->sh_addr;
      size = shdr->sh_size;
    }
  }
  elf->segments = CAllocAligned(lastAddr+size, 64);
#ifdef DEBUG
  CommPrint("segment|addr:0x%x size:%u\n", elf->segments, lastAddr+size);
#endif
  shdr = elf->u8 + elf->ehdr->e_shoff;
  for (i=0; i<elf->ehdr->e_shnum; i++, shdr++) {
    U8* name = shstrtab + shdr->sh_name;
#ifdef DEBUG
    CommPrint("[SECTION] name:%s type:%u flags:%u addr:0x%x offset:0x%x\n",
              name, shdr->sh_type, shdr->sh_flags, shdr->sh_addr, shdr->sh_offset);
#endif
    if (shdr->sh_type == SHT_NOBITS) { // just BSS afaik
      MemSet(elf->segments + shdr->sh_addr, 0, shdr->sh_size);
#ifdef DEBUG
      CommPrint("Zeroing %s addr:0x%06x size:%d\n", name, shdr->sh_addr, shdr->sh_size);
#endif
    } else {
      if (!StrCmp(name, ".symtab")) {
        elf->symtab = elf->u8 + shdr->sh_offset;
        elf->symtab_size = shdr->sh_size;
      } else if (!StrCmp(name, ".strtab")) {
        elf->strtab = elf->u8 + shdr->sh_offset;
        elf->strtab_size = shdr->sh_size;
      }
      if (shdr->sh_addr) {
#ifdef DEBUG
        CommPrint("Copy section %s addr:0x%06x size:%d\n", name, shdr->sh_addr, shdr->sh_size);
#endif
        MemCpy(elf->segments + shdr->sh_addr, elf->u8 + shdr->sh_offset, shdr->sh_size);
        if (!StrCmp(name, ".dynstr")) {
          elf->dynstr = elf->segments + shdr->sh_addr;
        } else if (!StrCmp(name, ".dynsym")) {
          elf->dynsym = elf->segments+shdr->sh_addr;
          elf->dynsym_size = shdr->sh_size / shdr->sh_entsize;
        } else if (!StrCmp(name, ".plt")) {
          elf->plt = elf->segments+shdr->sh_addr;
        } else if (!StrCmp(name, ".rela.dyn")) {
          elf->rela_dyn = elf->segments+shdr->sh_addr;
          elf->rela_dyn_size = shdr->sh_size / shdr->sh_entsize;
        } else if (!StrCmp(name, ".rela.plt")) {
          elf->rela_plt = elf->segments+shdr->sh_addr;
          elf->rela_plt_size = shdr->sh_size / shdr->sh_entsize;
        }
      }
    }
  }
}

U0 process_elf_rela_dyn_entries(Elf *elf) {
  I64 i, ii;
  U8 *entry_name;
  U64* got_addr;
  RELA_entry *rela_dyn = elf->rela_dyn;
  for (i = 0; i < elf->rela_dyn_size; i++, rela_dyn++) {
    entry_name = elf->dynstr + elf->dynsym[(rela_dyn->r_info >> 32)].st_name;
#ifdef DEBUG
    CommPrint("[rela.dyn] name:%s offset:0x%x info:%x\n", entry_name, rela_dyn->r_offset, rela_dyn->r_info);
#endif
    if (rela_dyn->r_info == 8) {
#ifdef DEBUG
      CommPrint("[rela.dyn] type: %d\n",  rela_dyn->r_info);
#endif
      got_addr = elf->segments + rela_dyn->r_offset;
      *got_addr += elf->segments;
#ifdef DEBUG
      CommPrint("[rela.dyn] name:%p 0x%x\n", got_addr, *got_addr);
#endif
    } else if (!StrCmp(entry_name, "stdin")) {
      *(rela_dyn->r_offset)(U64 *) = 0;
#ifdef DEBUG
      CommPrint("Set value for .rela.dyn entry '%s' to: %d\n", entry_name, 0);
#endif
    } else if (!StrCmp(entry_name, "stdout")) {
      *(rela_dyn->r_offset)(U64 *) = 1;
#ifdef DEBUG
      CommPrint("Set value for .rela.dyn entry '%s' to: %d\n", entry_name, 1);
#endif
    } else if (!StrCmp(entry_name, "stderr")) {
      *(rela_dyn->r_offset)(U64 *) = 2;
#ifdef DEBUG
      CommPrint("Set value for .rela.dyn entry '%s' to: %d\n", entry_name, 2);
#endif
    } else if (entry_name[0] != '\0') {
#ifdef DEBUG
      CommPrint(".rela.dyn entry: %s 0x%x\n", entry_name, (elf->segments+rela_dyn->r_offset)[0]);
#endif
      U8 entry_split[128];
      symbol_split(entry_name, entry_split);
      CHashClass* addr = get_symbol_address(entry_split);
      if (addr) {
#ifdef DEBUG
        CommPrint("patch: %x %s %x\n", elf->segments+rela_dyn->r_offset, entry_split, addr);
#endif
        *((elf->segments+rela_dyn->r_offset)(U64*)) = addr;
      }
      Elf64_Sym* dynsym = elf->dynsym;
      for (ii = 0; ii<elf->dynsym_size; ii++, dynsym++) {
        U8* dynsym_entry_name = elf->dynstr + dynsym->st_name;
        if (!StrCmp(dynsym_entry_name, entry_name)) {
          got_addr = elf->segments + rela_dyn->r_offset;
          U64* got_value = elf->segments + dynsym->st_value;
#ifdef DEBUG
          CommPrint("[rela.dyn] name:%s offset:0x%x value:0x%x got_addr:0x%x got_value:0x%x\n", entry_name, rela_dyn->r_offset, dynsym->st_value, got_addr, got_value);
#endif
          *got_addr = got_value;
          break;
        }
      }
    }
  }
}

U0 process_elf_dynsym_entries(Elf *elf) {
  I64 i;
  U8* entry_name;
  Elf64_Sym* dynsym = elf->dynsym;
  for (i = 0; i<elf->dynsym_size; i++) {
    entry_name = elf->dynstr + dynsym->st_name;
#ifdef DEBUG
    CommPrint("[dynsym] name:%s value:0x%x\n", entry_name, dynsym->st_value);
#endif
    if (!StrCmp(entry_name, "init")) {
      _init = dynsym->st_value;
    } else if (!StrCmp(entry_name, "loop")) {
      _loop = dynsym->st_value;
    } else if (!StrCmp(entry_name, "check_if_quitting")) {
      _check_if_quitting = dynsym->st_value;
    } else if (!StrCmp(entry_name, "quit")) {
      _quit = dynsym->st_value;
    }
    dynsym++;
  }
}

/*
U0 process_elf_debug_symbols(Elf *elf) {
  I64 i = 0;
  I64 entry_bind;
  U64 entry_name;
  I64 entry_type;
  CHashFun *hf;
  CHashGlblVar *hgv;
  Elf64_Sym *symtab = elf->symtab;
  entry_name = elf->strtab;
  entry_name += symtab->st_name;
  while (i < elf->symtab_size) {
    entry_bind = symtab->st_info >> 4;
    entry_type = symtab->st_info & 0xf;
    switch (entry_type) {
    case STT_OBJECT:
      hgv = CAllocAligned(sizeof(CHashGlblVar), 16);
      hgv->str = entry_name;
      hgv->type = HTT_GLBL_VAR;
      hgv->data_addr = symtab->st_value;
      hgv->size = symtab->st_size;
      // TempleOS reboots if I nest a switch table here, for some reason, so we
      // have to do this dumb shit instead...
      hgv->var_class = get_symbol_hash_entry("U32");
      if (hgv->size == 1)
        hgv->var_class = get_symbol_hash_entry("U8");
      if (hgv->size == 2)
        hgv->var_class = get_symbol_hash_entry("U16");
      HashAdd(hgv, Fs->hash_table);
#ifdef DEBUG
      CommPrint(
          "Add global variable '%s' to hash table, addr = 0x%08x, size = %d\n",
          entry_name, symtab->st_value, symtab->st_size);
#endif
      break;
    case STT_FUNC:
      hf = CAllocAligned(sizeof(CHashFun), 16);
      hf->str = entry_name;
      hf->type = HTT_FUN;
      hf->exe_addr = symtab->st_value;
      hf->size = symtab->st_size;
      HashAdd(hf, Fs->hash_table);
#ifdef DEBUG
      CommPrint("Add function '%s' to hash table, addr = 0x%08x, size = %d\n",
                  entry_name, symtab->st_value, symtab->st_size);
#endif
      break;
    }
    symtab++;
    i++;
    entry_name = elf->strtab;
    entry_name += symtab->st_name;
  }
}
*/

U0 process_elf_rela_plt_entries(Elf *elf) {
  I64 i, ii;
  U32 handler;
  U8 *entry_name;
  PLT_entry *plt = elf->plt;
  RELA_entry *rela_plt = elf->rela_plt;
  plt++;
  for (i = 0; i < elf->rela_plt_size; i++) {
    entry_name = elf->dynstr + elf->dynsym[(rela_plt->r_info >> 32)].st_name;
    U8 entry_split[128];
    symbol_split(entry_name, entry_split);
#ifdef DEBUG
    CommPrint("[rela.plt] name:%s plt:0x%x split:%s offset:0x%x\n",entry_name,plt,entry_split,rela_plt->r_offset);
    handler = CAllocAligned(sizeof(unimplemented_symbol), 16, Fs->code_heap);
    MemCpy(handler, &unimplemented_symbol, sizeof(unimplemented_symbol));
    U32 *patch = handler + 0x0A;
    *patch = entry_name;
#endif
    U64* got_addr = elf->segments+rela_plt->r_offset;
    CHashClass* addr = get_symbol_address(entry_split);
    if (addr) {
      *got_addr = addr;
#ifdef DEBUG
      CommPrint("[got.plt] name:%s addr:%x value:%x\n", entry_name, got_addr, *got_addr);
#endif
    } else {
      Elf64_Sym* dynsym = elf->dynsym;
      for (ii = 0; ii<elf->dynsym_size; ii++, dynsym++) {
        entry_name = elf->dynstr + dynsym->st_name;
        if (!StrCmp(entry_name, entry_split)) {
#ifdef DEBUG
          CommPrint("[rela.plt] name:%s src_addr:%x dst_addr:%x\n", entry_name, dynsym->st_value, elf->segments+dynsym->st_value);
#endif
          *got_addr = elf->segments+dynsym->st_value;
/*
          if (!StrCmp(entry_name, "new_mob")) {
            CommPrint("Breakpoint Set: %x\n", got_addr);
            BptS(*got_addr);
          }
*/
          break;
        }
      }
    }
    rela_plt++;
    plt++;
  }
}

U0 load_elf(...) {
  if (!FileFind(argv[0])) {
#ifdef DEBUG
    CommPrint("[load_elf] File not found: %s\n", argv[0]);
#endif
    return;
  }

  Elf elf;
  elf.u8 = FileRead(argv[0], &elf.size);
#ifdef DEBUG
  CommPrint("[load_elf] file:%s size:%d bytes\n", argv[0], elf.size);
#endif

  if (!is_valid_elf(&elf)) {
#ifdef DEBUG
    CommPutS("[load_elf] Not a valid ELF x86-64 executable\n");
#endif
    return;
  }

  process_elf_section_header_table(&elf);
  process_elf_dynsym_entries(&elf);
  process_elf_rela_dyn_entries(&elf);
  process_elf_rela_plt_entries(&elf);
  //process_elf_debug_symbols(&elf);

  _init = elf.segments + _init;
  _loop = elf.segments + _loop;
  _check_if_quitting = elf.segments + _check_if_quitting;
  _quit = elf.segments + _quit;

#ifdef DEBUG
  CommPrint("[load_elf] Done entry:0x%X file:0x%x\n", _init, elf);
#endif
}
