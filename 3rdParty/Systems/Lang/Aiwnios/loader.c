#include "aiwn.h"
#include <string.h>
typedef struct _CHashImport {
  CHash base;
  char *module_base;
  char *module_header_entry;
} _CHashImport;

#define IET_END 0
// reserved
#define IET_REL_I0      2 // Fictitious
#define IET_IMM_U0      3 // Fictitious
#define IET_REL_I8      4
#define IET_IMM_U8      5
#define IET_REL_I16     6
#define IET_IMM_U16     7
#define IET_REL_I32     8
#define IET_IMM_U32     9
#define IET_REL_I64     10
#define IET_IMM_I64     11
#define IEF_IMM_NOT_REL 1
// reserved
#define IET_REL32_EXPORT     16
#define IET_IMM32_EXPORT     17
#define IET_REL64_EXPORT     18 // Not implemented
#define IET_IMM64_EXPORT     19 // Not implemented
#define IET_ABS_ADDR         20
#define IET_CODE_HEAP        21 // Not really used
#define IET_ZEROED_CODE_HEAP 22 // Not really used
#define IET_DATA_HEAP        23
#define IET_ZEROED_DATA_HEAP 24 // Not really used
#define IET_MAIN             25

static void LoadOneImport(char **_src, char *module_base, int64_t ld_flags) {
  char        *src = *_src, *ptr2, *st_ptr;
  int64_t      i, etype;
  CHashExport *tmpex = NULL;
  int64_t      first = 1;
  // GNU extension, copied from TINE(https://github.com/eb-lan/TINE)
  // it compiles down to a mov call anyway so it doesn't hurt speed
#define READ_NUM(x, T)                                                         \
  ({                                                                           \
    T ret;                                                                     \
    memcpy(&ret, x, sizeof(T));                                                \
    ret;                                                                       \
  })
  while (etype = *src++) {
    i = READ_NUM(src, int32_t);
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    if (*st_ptr) {
      if (!first) {
        *_src = st_ptr - 5;
        return;
      } else {
        first = 0;
        if (!(tmpex =
                  HashFind(st_ptr, Fs->hash_table,
                           HTT_FUN | HTT_GLBL_VAR | HTT_EXPORT_SYS_SYM, 1))) {
          printf("Unresolved Reference:%s\n", st_ptr);
          _CHashImport *tmpiss;
          *(tmpiss = A_MALLOC(sizeof(_CHashImport), NULL)) = (_CHashImport){
              .base =
                  {
                      .str  = A_STRDUP(st_ptr, NULL),
                      .type = HTT_IMPORT_SYS_SYM2,
                  },
              .module_header_entry = st_ptr - 5,
              .module_base         = module_base,
          };
          HashAdd(tmpiss, Fs->hash_table);
        }
      }
    }
// same thing as above(avoiding strict aliaing stuff).
#define OFF(T) (i - (int64_t)ptr2 - sizeof(T))
#define REL(T)                                                                 \
  {                                                                            \
    size_t off = OFF(T);                                                       \
    memcpy(ptr2, &off, sizeof(T));                                             \
  }
#define IMM(T)                                                                 \
  { memcpy(ptr2, &i, sizeof(T)); }
    if (tmpex) {
      ptr2 = module_base + i;
      if (tmpex->base.type & HTT_FUN)
        i = ((CHashFun *)tmpex)->fun_ptr;
      else if (tmpex->base.type & HTT_GLBL_VAR)
        i = ((CHashGlblVar *)tmpex)->data_addr;
      else
        i = tmpex->val;
      switch (etype) {
      case IET_REL_I8:
        REL(char);
        break;
      case IET_REL_I16:
        REL(int16_t);
        break;
      case IET_REL_I32:
        REL(int32_t);
        break;
      case IET_REL_I64:
        REL(int64_t);
        break;
      case IET_IMM_U8:
        IMM(char);
        break;
      case IET_IMM_U16:
        IMM(int16_t);
        break;
      case IET_IMM_U32:
        IMM(int32_t);
        break;
      case IET_IMM_I64:
        IMM(int64_t);
        break;
      }
#undef OFF
#undef REL
#undef IMM
    }
  }
  *_src = src - 1;
}

static void SysSymImportsResolve2(char *st_ptr, int64_t ld_flags) {
  _CHashImport *tmpiss;
  char         *ptr;
  while (tmpiss = HashSingleTableFind(st_ptr, Fs->hash_table,
                                      HTT_IMPORT_SYS_SYM2, 1)) {
    ptr = tmpiss->module_header_entry;
    LoadOneImport(&ptr, tmpiss->module_base, ld_flags);
    tmpiss->base.type = HTT_INVALID;
  }
}

static void LoadPass1(char *src, char *module_base, int64_t ld_flags) {
  char        *ptr2, *ptr3, *st_ptr;
  int64_t      i, j, cnt, etype;
  CHashExport *tmpex = NULL;
  while (etype = *src++) {
    i = READ_NUM(src, int32_t);
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_REL32_EXPORT:
    case IET_IMM32_EXPORT:
    case IET_REL64_EXPORT:
    case IET_IMM64_EXPORT:
      if (etype != IET_IMM32_EXPORT || etype != IET_IMM64_EXPORT)
        i += (intptr_t)module_base;
      *(tmpex = A_MALLOC(sizeof(CHashExport), NULL)) = (CHashExport){
          .base =
              {
                  .str  = A_STRDUP(st_ptr, NULL),
                  .type = HTT_EXPORT_SYS_SYM,
              },
          .val = i,
      };
      HashAdd(tmpex, Fs->hash_table);
      SysSymImportsResolve2(st_ptr, ld_flags);
      break;
    case IET_REL_I0 ... IET_IMM_I64:
      src = st_ptr - 5;
      LoadOneImport(&src, module_base, ld_flags);
      break;
    case IET_ABS_ADDR:
      cnt = i;
      for (j = 0; j < cnt; j++) {
        ptr2 = module_base + READ_NUM(src, int32_t);
        src += 4;
        // Changed to 64bit by nroot
        int64_t off;
        memcpy(&off, ptr2, sizeof(int64_t));
        off += (intptr_t)module_base;
        memcpy(ptr2, &off, sizeof(int64_t));
      }
      break;
    case IET_CODE_HEAP:
    case IET_ZEROED_CODE_HEAP:
      ptr3 = A_MALLOC(READ_NUM(src, int32_t), NULL);
      src += 4;
    end:
      if (*st_ptr) {
        *(tmpex = A_MALLOC(sizeof(CHashExport), NULL)) = (CHashExport){
            .base =
                {
                    .str  = A_STRDUP(st_ptr, NULL),
                    .type = HTT_EXPORT_SYS_SYM,
                },
            .val = ptr3,
        };
        HashAdd(tmpex, Fs->hash_table);
      }
      cnt = i;
      for (j = 0; j < cnt; j++) {
        ptr2 = module_base + READ_NUM(src, int32_t);
        src += 4;
        int32_t off;
        memcpy(&off, ptr2, sizeof(int32_t));
        off += (intptr_t)ptr3;
        memcpy(ptr2, &off, sizeof(int32_t));
      }
      break;
    case IET_DATA_HEAP:
    case IET_ZEROED_DATA_HEAP:
      ptr3 = A_MALLOC(READ_NUM(src, int64_t), NULL);
      src += 8;
      goto end;
    }
  }
}

static void LoadPass2(char *src, char *module_base) {
  char   *st_ptr;
  int64_t i, etype;
  void (*fptr)();
  while (etype = *src++) {
    i = READ_NUM(src, int32_t);
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_MAIN:
      fptr = (i + module_base);
      FFI_CALL_TOS_0(fptr);
      break;
    case IET_ABS_ADDR:
      src += sizeof(int32_t) * i;
      break;
    case IET_CODE_HEAP:
    case IET_ZEROED_CODE_HEAP:
      src += 4 + sizeof(int32_t) * i;
      break;
    case IET_DATA_HEAP:
    case IET_ZEROED_DATA_HEAP:
      src += 8 + sizeof(int32_t) * i;
      break;
    }
  }
}

/*
class CBinFile
{//$LK,"Bin File Header Generation",A="FF:::/Compiler/CMain.HC,16 ALIGN"$ by
compiler. U16	jmp; U8	module_align_bits, reserved; U32	bin_signature; I64	org,
  patch_table_offset, //$LK,"Patch Table
Generation",A="FF:::/Compiler/CMain.HC,IET_ABS_ADDR"$ file_size;
};
 */
typedef struct __attribute__((packed)) CBinFile {
  uint16_t jmp;
  int8_t   module_align_bits, reserved;
  char     bin_signature[4];
  int64_t  org, patch_table_offset, file_size;
  char     data[];
} CBinFile;

char *Load(char *filename) { // Load a .BIN file module into memory.
  // bfh_addr==INVALID_PTR means don't care what load addr.
  char     *fbuf = filename, *module_base, *absname;
  int64_t   size, module_align, misalignment;
  CBinFile *bfh;
  CBinFile *bfh_addr;

  if (!(bfh = FileRead(fbuf, &size))) {
    return NULL;
  }

  // See $LK,"Patch Table Generation",A="FF:::/Compiler/CMain.HC,IET_ABS_ADDR"$
  module_align = 1 << bfh->module_align_bits;
  if (!module_align /*|| bfh->bin_signature != BIN_SIGNATURE_VAL*/) {
    A_FREE(bfh);
    throw(READ_NUM("BINM", int32_t));
  }
  bfh_addr = bfh;

lo_skip:
  LoadPass1((char *)bfh_addr + bfh_addr->patch_table_offset, bfh_addr->data, 0);
  LoadPass2((char *)bfh_addr + bfh_addr->patch_table_offset, bfh_addr->data);
  return bfh_addr;
}
#undef READ_NUM

void ImportSymbolsToHolyC(void (*cb)(char *name, void *addr)) {
  int64_t      idx = 0;
  CHashExport *h;
  for (idx = 0; idx <= Fs->hash_table->mask; idx++) {
    for (h = Fs->hash_table->body[idx]; h; h = h->base.next) {
      if (h->base.type & HTT_EXPORT_SYS_SYM) {
        FFI_CALL_TOS_2(cb, h->base.str, h->val);
      }
    }
  }
}
