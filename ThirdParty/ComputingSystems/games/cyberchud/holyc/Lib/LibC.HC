#ifndef LIBC_HC
#define LIBC_HC

#if 0
Cd(__DIR__);;
#include "CrunkComm"
InitComm();
#endif

CTask *LibC_MainTask = Fs();
I64 malloc_mutex=0;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

U64 find_u32_in_memory(U64 offset, U32 value) {
  I64 i = 0;
  while (MemCmp(offset + i, &value, 4))
    i++;
  return offset + i;
}

class FILE {
  U8 *filename;
  U8 *data;
  U8 *mode;
  I64 descriptor;
  I64 size;
  I64 offset;
  I64 pos;
};

class va_list {
  U32 gp_offset;
  U32 fp_offset;
  U8 *overflow_arg_area;
  U8 *reg_save_area;
};

#define PUSH_SYSV_REGS asm {PUSH RBX PUSH R12 PUSH R13 PUSH R14 PUSH R15}
#define POP_SYSV_REGS asm {POP R15 POP R14 POP R13 POP R12 POP RBX}

#define MOV_ANS_RAX asm {MOV [&ans], RAX}

I64 new_fd = 3;

#define stdin 0
#define stdout 1
#define stderr 2

U8* @calloc(U64 nmemb, U64 size) {
#ifdef DEBUG
  CommPrint("[calloc] task:%x nmemb:%u, size:%u\n", Fs, nmemb, size);
#endif
  U8* ptr = CAllocAligned(nmemb * size, 64);
#ifdef DEBUG
  CommPrint("[calloc done] task:%x nmemb:%u, size:%u ptr:%x\n", Fs, nmemb, size, ptr);
#endif
  return ptr;
}
U0 calloc() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@calloc
  }
  POP_SYSV_REGS
}

I64 @fclose(FILE *f) {
#ifdef DEBUG
  CommPrint("[fclose] descriptor:%d\n", f->descriptor);
#endif
  if (!f)
    return NULL;
  if (f->data)
    Free(f->data);
  if (f->filename)
    Free(f->filename);
  if (f->mode)
    Free(f->mode);
  Free(f);
  return NULL;
}
U0 fclose() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@fclose
  }
  POP_SYSV_REGS
}

FILE *@fopen(U8* filename, U8* mode) {
#ifdef DEBUG
  CommPrint("[fopen] name:%s mode:%s\n", filename, mode);
#endif
  U64 file_data;
  FILE *f;
  if (!StrCmp(mode, "r") || !StrCmp(mode, "r+") || !StrCmp(mode, "rb")) {
    if (!FileFind(filename))
      return NULL;
    f = CAllocAligned(sizeof(FILE), 64);
    f->filename = StrNew(filename);
    file_data = FileRead(filename, &f->size);
    f->data = MAllocAligned(f->size, 64);
    MemCpy(f->data, file_data, f->size);
    f->descriptor = new_fd++;
    f->mode = StrNew(mode);
#ifdef DEBUG
    CommPrint("[fopen] got descriptor:%d\n", f->descriptor);
#endif
    return f;
  }
}
U0 fopen() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@fopen
  }
  POP_SYSV_REGS
}

I64 @fread(U8 *ptr, I64 size, I64 nmemb, FILE *f) {
#ifdef DEBUG
  CommPrint("[fread] ptr:0x%08x size:%d nmemb:%d descriptor:%d\n",ptr,size,nmemb,f->descriptor);
#endif
  MemCpy(ptr, f->data + f->pos, (size * nmemb));

  //"fread @ pos, size: %d, %d\n", f->pos, (size * nmemb);
  // D(ptr, Min(256, size * nmemb));

  f->pos += (size * nmemb);
  return (size * nmemb);
}
U0 fread() {
  PUSH_SYSV_REGS
  asm {
    PUSH RCX
    PUSH RDX
    PUSH RSI
    PUSH RDI
    CALL &@fread
  }
  POP_SYSV_REGS
}

U0 free() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &Free
  }
  POP_SYSV_REGS
}

I64 @fseek(FILE *f, I64 off, I64 whence) {
#ifdef DEBUG
  CommPrint("[fseek] desciptor:%d off:%d whence:%d\n",f->descriptor,off,whence);
#endif
  switch (whence) {
  case SEEK_SET:
    f->pos = off;
    break;
  case SEEK_CUR:
    f->pos += off;
    break;
  case SEEK_END:
    f->pos = f->size + off;
    break;
  }
  return NULL;
}

U0 fseek() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDX
    PUSH RSI
    PUSH RDI
    CALL &@fseek
  }
  POP_SYSV_REGS
}

I64 @ftell(FILE *f) {
#ifdef DEBUG
  CommPrint("[ftell] file:%x pos:%d\n", f, f->pos);
#endif
  return f->pos;
}
U0 ftell() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@ftell
  }
  POP_SYSV_REGS
}

/*
I64 @fwrite(U8* ptr, I64 size, I64 nmemb, FILE* f) {
  CommPrint("fwrite(0x%08x %d %d 0x%08x)\n",ptr,size,nmemb,f->descriptor);
  if (f->descriptor(U64) >= 0x100000000) {
    // mock for stderr problem
    PutS(p0);
    "\n";
  }
  if (f->descriptor(U64) >= 0x100000000)
    return 0;

  MemCpy(f->data + f->pos, ptr, (size * nmemb));
//  if (debug.enabled)
//    D(f->data + f->pos, Min(256, size * nmemb));
  f->pos += (size * nmemb);
  CommPrint("fwrite: pos: %d\n", f->pos);
  return size * nmemb;
}
U0 fwrite() {
  PUSH_SYSV_REGS
  asm {
    PUSH RCX
    PUSH RDX
    PUSH RSI
    PUSH RDI
    CALL &@fwrite
  }
  POP_SYSV_REGS
}
*/

U64 @malloc(U64 size) {
  U64 ptr = NULL;
#ifdef DEBUG
  CommPrint("[malloc] task:%x size:%u\n", Fs, size);
#endif
  ptr = MAllocAligned(size, 64);
#ifdef DEBUG
  CommPrint("[malloc result] task:%x size:%u ptr:%x\n", Fs, size, ptr);
#endif
  return ptr;
}
U0 malloc() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@malloc
  }
  POP_SYSV_REGS
}

U64 @aligned_alloc(U64 alignment, U64 size) {
  U64 ptr = NULL;
  ptr = MAllocAligned(size, alignment);
#ifdef DEBUG
  CommPrint("[aligned_alloc] size:%u align:%u ptr:0x%08x\n", size, alignment, ptr);
#endif
  return ptr;
}
U0 aligned_alloc() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@aligned_alloc
  }
  POP_SYSV_REGS
}

I32 @fputs(U8* s) {
  CommPutS(s);
  return 0;
}
U0 fputs() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@fputs
  }
  POP_SYSV_REGS
}

U8* @realloc(U8 *ptr, I64 size) {
#ifdef DEBUG
  CommPrint("[realloc] main:%x task:%x ptr:%x size:%d\n", LibC_MainTask, Fs, ptr, size);
#endif
  U8* new;
  if (!ptr) {
#ifdef DEBUG
    CommPrint("[realloc null] main:%x task:%x ptr:%x size:%d\n", LibC_MainTask, Fs, ptr, size);
#endif
    new = MAllocAligned(size, 64);
  } else {
#ifdef DEBUG
    CommPrint("[realloc resize] main:%x task:%x ptr:%x\n", LibC_MainTask, Fs, ptr);
#endif
    new = MAllocAligned(size, 64);
    I64 old_size = MSize(ptr);
#ifdef DEBUG
    CommPrint("[realloc memcpy] main:%x task:%x ptr:%x old_size:%d size:%d new:%x\n", LibC_MainTask, Fs, ptr, old_size, size, new);
#endif
    MemCpy(new, ptr, old_size);
#ifdef DEBUG
    CommPrint("[realloc free] main:%x task:%x ptr:%x size:%d new:%x\n", LibC_MainTask, Fs, ptr, size, new);
#endif
    Free(ptr);
  }
  return new;
}
U0 realloc() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@realloc
  }
  POP_SYSV_REGS
}

/* sleep in microseconds */
/* this is a lie it's just a raw divisor */
//#define PIT_FREQ 1193182
// DIVISOR = PIT_FREQ/SAMPLING_RATE
U0 @usleep(U64 us) {
  asm {
     MOV RBX,10[RBP]
     MOV AL,0x34
     OUT 0x43,AL

     MOV AL,BL
     OUT 0x40,AL
     MOV AL,BH
     OUT 0x40,AL

     STI
     HLT
     CLI

     /* Reset Timer for TempleOS */
     MOV AL,0x34
     OUT 0x43,AL
     MOV RBX,SYS_TIMER0_PERIOD
     MOV AL,BL
     OUT 0x40,AL
     MOV AL,BH
     OUT 0x40,AL
  }
}
U0 usleep() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@usleep
  }
  POP_SYSV_REGS
}

class tos_time_t {
  I64* hpet_addr;
  U64 hpet_initial;
  F64 hpet_freq;
};

class tos_data_t {
  CCPU* ccpu;
  tos_time_t time;
  U8* vga;
  CBlkPool* bp_code;
  CBlkPool* bp_data;
};

U0 @get_tos_data(tos_data_t* data) {
#ifdef DEBUG
  CommPrint("[get_tos_data]\n");
#endif
  data->ccpu = cpu_structs;
  data->time.hpet_addr = dev.uncached_alias+HPET_MAIN_CNT;
  data->time.hpet_initial = cnts.HPET_initial;
  data->time.hpet_freq = cnts.HPET_freq;
  data->vga = text.vga_alias;
  data->bp_code = sys_code_bp;
  data->bp_data = sys_data_bp;
}
U0 get_tos_data() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@get_tos_data
  }
  POP_SYSV_REGS
}

U0 EnableSSE() {
  asm
  {
  MOV_EAX_CR0
  AND AX, 0xFFFB // clear coprocessor emulation CR0.EM
  OR AX, 0x2 // set coprocessor monitoring  CR0.MP
  MOV_CR0_EAX
  MOV_EAX_CR4
  OR EAX, 3 << 9 // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
  MOV_CR4_EAX
  }
}

#endif
