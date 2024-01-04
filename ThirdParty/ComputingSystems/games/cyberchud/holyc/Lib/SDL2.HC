#ifndef SDL2_HC
#define SDL2_HC

#if 0
Cd(__DIR__);;
#include "CrunkComm"
#include "LibC"
#include "SDL2ScanCodes"
#define VERBOSE
#endif

#define MAX_JOBS (mp_cnt*2+8)

/* MAX_EVENT_CNT should be large enough to fit maximum theoretical number of events */
#define MAX_EVENT_CNT 272

#define SDL_KEYDOWN 0x300
#define SDL_KEYUP   0x301

#define SDL_MOUSEMOTION     0x400
#define SDL_MOUSEBUTTONDOWN 0x401
#define SDL_MOUSEBUTTONUP   0x402
#define SDL_MOUSEWHEEL      0x403

#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3

#define SDL_RELEASED 0
#define SDL_PRESSED  1

#define SCREEN_W 640
#define SCREEN_H 480

#define PUSH_SYSV_REGS asm {PUSH RBX PUSH R12 PUSH R13 PUSH R14 PUSH R15}
#define POP_SYSV_REGS asm {POP R15 POP R14 POP R13 POP R12 POP RBX}

#define PUSH_R asm { PUSH RAX PUSH RSI PUSH RCX PUSH RDX PUSH RDI PUSH R8 PUSH R9 PUSH R10 PUSH R11 }
#define POP_R asm { POP R11 POP R10 POP R9 POP R8 POP RDI POP RDX POP RCX POP RSI POP RAX }

#define PUSH_R2 asm { PUSH RSI PUSH RCX PUSH RDX PUSH RDI PUSH R8 PUSH R9 PUSH R10 PUSH R11 }
#define POP_R2 asm { POP R11 POP R10 POP R9 POP R8 POP RDI POP RDX POP RCX POP RSI }

#define PUSH_TOS_R { PUSH RBP PUSH RSI PUSH RDI PUSH R10 PUSH R11 }
#define POP_TOS_R { POP R11 POP R10 POP RDI POP RSI POP RBP }

/* SDL2 uses 58/64 (depending on architecture) sized events, but currently the events we support don't use more than 36 */
class SDL_Event {
  I64 pad[5];
};

class SDL_Keysym {
  U32 scancode;
  I32 sym;
  U16 mod;
  U32 unused;
};

class SDL_KeyboardEvent {
  U32 type;
  U32 timestamp;
  U32 windowID;
  U8 state;
  U8 repeat;
  U8 pad0;
  U8 pad1;
  SDL_Keysym keysym;
};

class SDL_MouseButtonEvent {
  U32 type;
  U32 timestamp;
  U32 windowID;
  U32 which;
  U8 button;
  U8 state;
  U8 clicks;
  U8 pad;
  I32 x;
  I32 y;
};

class SDL_MouseMotionEvent {
  U32 type;
  U32 timestamp;
  U32 windowID;
  U32 which;
  U32 state;
  I32 x;
  I32 y;
  I32 xrel;
  I32 yrel;
};

class SDL_Rect {
  I32 x, y;
  I32 w, h;
};

class SDL_Color {
  U8 r;
  U8 g;
  U8 b;
  U8 a;
};

class SDL_Palette {
  I32 ncolors;
  SDL_Color *colors;
  U32 version;
  I32 refcount;
};

class SDL_PixelFormat {
  U32 format;
  SDL_Palette *palette;
  U8 BitsPerPixel;
  U8 BytesPerPixel;
  U8 padding[2];
  U32 Rmask;
  U32 Gmask;
  U32 Bmask;
  U32 Amask;
  U8 Rloss;
  U8 Gloss;
  U8 Bloss;
  U8 Aloss;
  U8 Rshift;
  U8 Gshift;
  U8 Bshift;
  U8 Ashift;
  I32 refcount;
  SDL_PixelFormat* next;
};

class SDL_BlitMap {
  I64 placeholder;
};

class SDL_Surface {
  U64 flags; // U64 for C alignment
  SDL_PixelFormat *format;
  I32 w;
  I32 h;
  I32 pitch;
  I32 pad0; // C alignment
  U8* pixels;
  U8* userdata;
  I32 locked;
  U8* list_blitmap;
  SDL_Rect clip_rect;
  SDL_BlitMap *map;
  I32 refcount;
};

class SDL_Window {
  SDL_Surface surface;
  U8 *frontbuffer;
};

class SDL_AudioSpec {
  I32 freq;
  U16 format;
  U8 channels;
  U8 silence;
  U16 samples;
  U16 padding;
  U32 size;
  U0 (*callback)(U8*,U8*,I32);
  U8 *userdata;
};

class JobWrapData {
  I32 (*fn)(U8*);
  U8* name;
  U8* data;
  CTask* task;
  I64 used;
};

class @sdl {
  I16 mouse_x;
  I16 mouse_y;
  I16 mouse_abs_x;
  I16 mouse_abs_y;
  I16 lb;
  I16 rb;
  U64 ticks_init;
  SDL_Window win;
  I32 event_cnt;
  I32 event_pos;
  U8 key_state[256];
  SDL_Event events[MAX_EVENT_CNT];
  JobWrapData task_data[MAX_JOBS];
};

/* Globals */
@sdl sdl;

/* Functions */
I64 @SDL_Init() {
#ifdef DEBUG
  CommPutS("[TOS] [SDL_Init]\n");
#endif
  MemSet(&sdl, 0, sizeof(@sdl));
  MsSet(SCREEN_W/2, SCREEN_H/2);
  sdl.mouse_abs_x = SCREEN_W/2;
  sdl.mouse_abs_y = SCREEN_H/2;
  sdl.ticks_init = cnts.jiffies;
  return 0;
}

U0 SDL_Init() {
  PUSH_SYSV_REGS
  asm {
    CALL &@SDL_Init;
  }
  POP_SYSV_REGS
}

U0 @SDL_WarpMouseInWindow(I32 x, I32 y) {
#ifdef VERBOSE
  CommPrint("SDL_WarpMouseInWindow\n");
#endif
  MsSet(x, y);
}
U0 SDL_WarpMouseInWindow() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDX
    PUSH RSI
    CALL &@SDL_WarpMouseInWindow
  }
  POP_SYSV_REGS
}

I64 @SDL_GetRelativeMouseState(I32 *x, I32 *y) {
#ifdef VERBOSE
  CommPrint("[SDL_GetRelativeMouseState] tos:%dx%d abs:%dx%d rel:%dx%d\n", ms.pos.x, ms.pos.y, sdl.mouse_abs_x, sdl.mouse_abs_y, sdl.mouse_x, sdl.mouse_y);
#endif
  *x = sdl.mouse_x;
  *y = sdl.mouse_y;
  return 0; // TODO
}
U0 SDL_GetRelativeMouseState() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@SDL_GetRelativeMouseState
  }
  POP_SYSV_REGS
}

U64 @SDL_GetTicks64() { return ToF64((cnts.jiffies-sdl.ticks_init))/JIFFY_FREQ*1000; }
U0 SDL_GetTicks64() {
  PUSH_SYSV_REGS
  @SDL_GetTicks64();
  POP_SYSV_REGS
}

I32 @SDL_PollEvent(SDL_KeyboardEvent *event) {
  if (sdl.event_pos < sdl.event_cnt) {
    MemCpy(event, &sdl.events[sdl.event_pos++], sizeof(SDL_Event));
    return 1;
  }
  return 0;
}
U0 SDL_PollEvent() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_PollEvent
  }
  POP_SYSV_REGS
}

U32 sdl_get_sym_for_scancode(I64 sc) {
  switch (sc) {
  case SC_ENTER:
    return '\r';
    break;
  case SC_CURSOR_UP:
    return 273;
    break;
  case SC_CURSOR_DOWN:
    return 274;
    break;
  case SC_CURSOR_LEFT:
    return 276;
    break;
  case SC_CURSOR_RIGHT:
    return 275;
    break;
  case SC_SHIFT:
    return 304;
    break;
  case SC_CTRL:
    return 306;
    break;
  case SC_ALT:
    return 308;
    break;
  default:
    return ScanCode2Char(sc);
    break;
  }
}
extern U0 MsHardHndlr();
extern U0 MsHardPktRead();
extern U0 KbdBuildSC(U8 raw_byte,Bool in_irq,U8 *_last_raw_byte,I64 *_last_sc);

U0 MyKbdHndlr() {
  static U8 last_raw_byte=0;
  U8 raw_byte;
  FifoU8Rem(kbd.fifo2,&raw_byte);

  KbdBuildSC(raw_byte, FALSE, &last_raw_byte, &kbd.scan_code);
  if (raw_byte==0xE0) {
    FifoU8Rem(kbd.fifo2,&raw_byte);
    KbdBuildSC(raw_byte,FALSE,&last_raw_byte,&kbd.scan_code);
  }
  if (Btr(&kbd.scan_code,SCf_NEW_KEY)) {
    kbd.new_key_timestamp=kbd.timestamp;
    Btr(&kbd.scan_code,32+SCf_NEW_KEY);
    if (!(kbd.scan_code&SCF_KEY_UP)) {
      kbd.last_down_scan_code=kbd.scan_code;
      Bts(kbd.down_bitmap,kbd.scan_code.u8[0]);
      Bts(kbd.down_bitmap2,kbd.scan_code.u8[4]);
    } else {
      Btr(kbd.down_bitmap,kbd.scan_code.u8[0]);
      Btr(kbd.down_bitmap2,kbd.scan_code.u8[4]);
    }
  }
}

U0 @SDL_PumpEvents() {
#ifdef VERBOSE
  CommPutS("[SDL_PumpEvents]\n");
#endif

  /* MyKbdMsHndlr() */
  if (!ms_hard.irqs_working) {
    PUSHFD
    CLI
    while (InU8(KBD_CTRL)&1)
      MsHardPktRead();
    POPFD
  }

  while (FifoU8Cnt(kbd.fifo2))
    MyKbdHndlr();
  while (FifoU8Cnt(ms_hard.fifo2))
    MsHardHndlr();

  /* MyWinMsUpdate */
  I64 dd;
  Bool set=FALSE;
  ms.has_wheel=ms_hard.has_wheel;
  if (ms_hard.evt) {
    MsUpdate(ms_hard.pos.x,ms_hard.pos.y,ms_hard.pos.z,
      ms_hard.bttns[0],ms_hard.bttns[1]);
    ms_hard.evt=FALSE;
    set=TRUE;
  }

  if (set) {
    ms.speed=ms_hard.speed;
    ms.timestamp=ms_hard.timestamp;
  } else {
    ms.speed*=0.95;
  }

  /* Reset Mouse Position */
  sdl.event_cnt = 0;
  sdl.event_pos = 0;
  I32 centerx = SCREEN_W/2;
  I32 centery = SCREEN_H/2;
  if (ms.pos.x != centerx || ms.pos.y != centery) {
    sdl.mouse_x = ms.pos.x-centerx;
    sdl.mouse_y = ms.pos.y-centery;
    sdl.mouse_abs_x += sdl.mouse_x;
    if (sdl.mouse_abs_x >= SCREEN_W) sdl.mouse_abs_x = SCREEN_W-1;
    else if (sdl.mouse_abs_x < 0) sdl.mouse_abs_x = 0;

    sdl.mouse_abs_y += sdl.mouse_y;
    if (sdl.mouse_abs_y >= SCREEN_H) sdl.mouse_abs_y = SCREEN_H-1;
    else if (sdl.mouse_abs_y < 0) sdl.mouse_abs_y = 0;

    MsSet(centerx, centery);
    SDL_MouseMotionEvent* motion = &sdl.events[sdl.event_cnt++];
    motion->type = SDL_MOUSEMOTION;
    motion->x = sdl.mouse_abs_x;
    motion->y = sdl.mouse_abs_y;
    motion->xrel = sdl.mouse_x;
    motion->yrel = sdl.mouse_y;
  }

  /* Generate Mouse Event */
  SDL_MouseButtonEvent *mev;
  if (sdl.lb != ms.lb) {
    sdl.lb = ms.lb;
    mev = &sdl.events[sdl.event_cnt++];
    if (ms.lb) {
      mev->type = SDL_MOUSEBUTTONDOWN;
    } else {
      mev->type = SDL_MOUSEBUTTONUP;
    }
    mev->button = SDL_BUTTON_LEFT;
    mev->x = sdl.mouse_x;
    mev->y = sdl.mouse_y;
  }
  if (sdl.rb != ms.rb) {
    sdl.rb = ms.rb;
    mev = &sdl.events[sdl.event_cnt++];
    if (ms.rb) {
      mev->type = SDL_MOUSEBUTTONDOWN;
    } else {
      mev->type = SDL_MOUSEBUTTONUP;
    }
    mev->button = SDL_BUTTON_RIGHT;
    mev->x = sdl.mouse_x;
    mev->y = sdl.mouse_y;
  }

  /* Generate Keyboard Events */
  I64 i;
  for (i=0; i<256; i++) {
    Bool key_state = Bt(kbd.down_bitmap, i);
    if (key_state != sdl.key_state[i]) {
      sdl.key_state[i] = key_state;
      SDL_KeyboardEvent *event = &sdl.events[sdl.event_cnt++];
      switch (key_state) {
      case 1:
        event->type = SDL_KEYDOWN;
        event->state = SDL_PRESSED;
        event->keysym.scancode = templeos_scancode_table[i];
        break;
      case 0:
        event->type = SDL_KEYUP;
        event->state = SDL_RELEASED;
        event->keysym.scancode = templeos_scancode_table[i];
        break;
      }
    }
  }

  FlushMsgs();
}

U0 SDL_PumpEvents() {
  PUSH_SYSV_REGS
  asm {
    CALL &@SDL_PumpEvents
  }
  POP_SYSV_REGS
}

U0 SDL_DrawIt(CTask*, CDC *dc) {
  U64 off = SCREEN_W*8;
  MemCpy(dc->body+off, sdl.win.frontbuffer+off, SCREEN_W*SCREEN_H-off);
}

SDL_Window* @SDL_CreateWindow(U8 *title, I32 x, I32 y, I32 w, I32 h, U32 flags) {
#ifdef DEBUG
  CommPrint("[TOS] [SDL_CreateWindow] w:%d h:%d win:%x\n", w, h, &sdl.win);
#endif
  sdl.win.surface.w = 640;
  sdl.win.surface.h = 480;
  sdl.win.surface.pixels = MAllocAligned(w*h, 64);
  sdl.win.frontbuffer = MAllocAligned(w*h, 64);
//  Fs->draw_it = &SDL_DrawIt;
  return &sdl.win;
}
U0 SDL_CreateWindow() {
  PUSH_SYSV_REGS
  asm {
    PUSH R9
    PUSH R8
    PUSH RCX
    PUSH RDX
    PUSH RSI
    PUSH RDI
    CALL &@SDL_CreateWindow
  }
  POP_SYSV_REGS
}

SDL_Surface* @SDL_GetWindowSurface(SDL_Window* win) {
//  CommPrint("tos win:%x surface:%x pixels:%x\n", win, &win->surface, win->surface.pixels);
  return &win->surface;
}

U0 SDL_GetWindowSurface() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_GetWindowSurface
  }
  POP_SYSV_REGS
}

I64 @SDL_UpdateWindowSurface(SDL_Window* win) {
#ifdef VERBOSE
  CommPrint("[TOS] [TSDL_UpdateWindowSurface] win:%x\n", win);
#endif
  U8 *tmp = sdl.win.frontbuffer;
  sdl.win.frontbuffer = sdl.win.surface.pixels;
  sdl.win.surface.pixels = tmp;
  Refresh(1, TRUE);
  return 0;
}
U0 SDL_UpdateWindowSurface() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_UpdateWindowSurface
  }
  POP_SYSV_REGS
}

U0 SysVJobWrapper(JobWrapData* data) {
//  CommPrint("SysVJobWrapper: fn:%x name:%s data:%x\n", data->fn, data->name, data->data);
  PUSH_R2
  asm {
    // Enable SSE
    MOV_EAX_CR0
    AND AX, 0xFFFB // clear coprocessor emulation CR0.EM
    OR AX, 0x2 // set coprocessor monitoring  CR0.MP
    MOV_CR0_EAX
    MOV_EAX_CR4
    OR EAX, 3 << 9 // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    MOV_CR4_EAX

    MOV RSI,U64 0x10[RBP]
    MOV R10,U64 [RSI]
    MOV RDI,U64 0x10[RSI]

    PUSH RBP
    MOV RBP,RSP

    MOV RCX,RSP
    AND ECX,63
    JE SysVJobWrapperL0
    MOV EDX,64
    SUB EDX,ECX
    SUB RSP,RDX
SysVJobWrapperL0:
    CALL R10

    MOV RSP,RBP
    POP RBP
  }
  POP_R2
}

I64 @SDL_CreateThread(I32 (*fn)(U8*), U8 *name, U8* data) {
#ifdef DEBUG
  CommPrint("[TOS] [SDL_CreateThread] fn:%x name:%s data:%x\n", fn, name, data);
#endif
  I64 idx = 0;
//  if (mp_cnt > 1)
//    idx=1;
  for (; idx<MAX_JOBS; idx++) {
    if (!sdl.task_data[idx].used) {
      sdl.task_data[idx].used = 1;
      goto SDL_CreateThreadL0;
    }
  }
  CommPrint("[TOS] [ERR] [SDL_CreateThread] RAN OUT!!!\n");
  return NULL;
SDL_CreateThreadL0:
  I64 core = (idx+1)%mp_cnt;
#ifdef DEBUG
  CommPrint("[TOS] [SDL_CreateThread] fn:%x name:%s data:%x core:%d\n", fn, name, data, core);
#endif
  JobWrapData* wrapper = &sdl.task_data[idx];
  wrapper->fn = fn;
  wrapper->name = name;
  wrapper->data = data;
  wrapper->task = Spawn(&SysVJobWrapper, wrapper, name, core, 0);
  return 0x100+idx;
}
U0 SDL_CreateThread() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDX
    PUSH RSI
    PUSH RDI
    CALL &@SDL_CreateThread
  }
  POP_SYSV_REGS
}

U0 @SDL_WaitThread(I64 idx, I32* status) {
#ifdef VERBOSE
  CommPrint("WaitThread %d\n", idx);
#endif
  U64 ridx = idx - 0x100;
  DeathWait(&sdl.task_data[ridx].task);
  sdl.task_data[ridx].used = 0;
}
U0 SDL_WaitThread() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@SDL_WaitThread
  }
  POP_SYSV_REGS
}

class SDL_mutex {
  I64 bit;
};

#define COND_MAX (MAX_JOBS*2)
class SDL_cond {
  SDL_mutex mu;
  I64 wakeup[COND_MAX]; // TODO dynamic
  I64 waiting[COND_MAX]; // TODO dynamic
};

I32 @SDL_LockMutex(SDL_mutex* mutex) {
#ifdef VERBOSE
  CommPrint("Lock Mutex, %d\n", mutex->bit);
#endif
  while (LBts(&mutex->bit, 0)) Yield;
//  while (LBts(&mutex->bit, 0)) SleepUntil(cnts.jiffies);
  return 0;
}
U0 SDL_LockMutex() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_LockMutex
  }
  POP_SYSV_REGS
}

I32 @SDL_UnlockMutex(SDL_mutex* mutex) {
#ifdef VERBOSE
  CommPutS("Unlock Mutex\n");
#endif
  if (mutex->bit == 0)
    CommPrint("DoubleUnlock\n");
  while (!LBtr(&mutex->bit, 0)) Yield;
  return 0;
}
U0 SDL_UnlockMutex() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_UnlockMutex
  }
  POP_SYSV_REGS
}

SDL_mutex* @SDL_CreateMutex() {
  SDL_mutex* mu = CAllocAligned(sizeof(SDL_mutex), 64, LibC_MainTask->data_heap);
  return mu;
}
U0 SDL_CreateMutex() {
  PUSH_SYSV_REGS
  @SDL_CreateMutex();
  POP_SYSV_REGS
}

SDL_cond* @SDL_CreateCond() {
  SDL_cond* cond = CAllocAligned(sizeof(SDL_cond), 64, LibC_MainTask->data_heap);
  return cond;
}
U0 SDL_CreateCond() {
  PUSH_SYSV_REGS
  @SDL_CreateCond();
  POP_SYSV_REGS
}

I32 @SDL_CondWait(SDL_cond* cond, SDL_mutex* mu) {
  I64 i;
  @SDL_LockMutex(&cond->mu);
  for (i=0; i<COND_MAX; i++) {
    if (cond->waiting[i] == 0) {
      cond->waiting[i] = 1;
      cond->wakeup[i] = 0;
      goto @SDL_CondWaitL0;
    }
  }
  CommPutS("[TOS] [SDL_CondWait] [ERR] FATAL\n");
  return -1;
@SDL_CondWaitL0:
  cond->mu.bit = 0;
  mu->bit = 0;
  /* yielding a spinlock, it pins the CPU */
  /* cnts.jiffies causes the sleep to return <1ms */
  while (!cond->wakeup[i]) SleepUntil(cnts.jiffies);
//  while (!cond->wakeup[i]) Yield;
  @SDL_LockMutex(mu);
  cond->waiting[i] = 0;
  return 0;
}
U0 SDL_CondWait() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@SDL_CondWait
  }
  POP_SYSV_REGS
}

U0 SDL_GenericElfFuncCaller(U8* callback) {
  MOV U64 RAX,U64 0x10[RBP]
  PUSH_R2
  asm {
    PUSH RBP
    MOV RBP,RSP

    MOV RCX,RSP
    AND ECX,15
    JE @@generic_init_aligned
    MOV EDX,16
    SUB EDX,ECX
    SUB RSP,RDX
@@generic_init_aligned:
    SUB RSP,0x10
    MOV 00[RSP],RAX
    CALL U64 00[RSP]
    MOV RSP,RBP
    POP RBP
  }
  POP_R2
}

I32 @CondWaitCallback(SDL_cond* cond, SDL_mutex* mu, U8* callback) {
  I64 i;
  @SDL_LockMutex(&cond->mu);
  for (i=0; i<COND_MAX; i++) {
    if (cond->waiting[i] == 0) {
      cond->waiting[i] = 1;
      cond->wakeup[i] = 0;
      goto @@CondWaitCallbackL0;
    }
  }
  CommPutS("[TOS] [SDL_CondWait] [ERR] FATAL\n");
  return -1;
@@CondWaitCallbackL0:
  cond->mu.bit = 0;
  mu->bit = 0;
  /* yielding a spinlock, it pins the CPU */
  /* cnts.jiffies causes the sleep to return <1ms */
  while (!cond->wakeup[i]) {
    SDL_GenericElfFuncCaller(callback);
    SleepUntil(cnts.jiffies);
  }
//  while (!cond->wakeup[i]) Yield;
  @SDL_LockMutex(mu);
  cond->waiting[i] = 0;
  return 0;
}
U0 CondWaitCallback() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDX
    PUSH RSI
    PUSH RDI
    CALL &@CondWaitCallback
  }
  POP_SYSV_REGS
}

I32 @SDL_CondSignal(SDL_cond* cond) {
#ifdef VERBOSE
  CommPrint("CondSignal %x\n", cond);
#endif
  @SDL_LockMutex(&cond->mu);
  cond->wakeup[0] = 1;
  cond->mu.bit = 0;
  return 0;
}

I32 @SDL_CondBroadcast(SDL_cond* cond) {
#ifdef VERBOSE
  CommPrint("CondBroad %x\n", cond);
#endif
  I64 i=0;
  @SDL_LockMutex(&cond->mu);
  for (i=0; i<COND_MAX; i++) {
    cond->wakeup[i] = 1;
  }
  cond->mu.bit = 0;
  return 0;
}
U0 SDL_CondBroadcast() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_CondBroadcast
  }
  POP_SYSV_REGS
}

U0 SDL_CondSignal() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_CondBroadcast
  }
  POP_SYSV_REGS
}

class SDL_AudioThreadData {
  U0 (*callback)(U8*,U8*,I32);
  U8 *userdata;
  I32 pos;
  I32 buf_len;
  U32 freq;
  U32 quit;
  I16 *buf;
};
SDL_AudioThreadData g_aud;

U0 @SDL_Quit() {
  g_aud.quit = 1;
  Free(sdl.win.surface.pixels);
  Free(sdl.win.frontbuffer);
}
U0 SDL_Quit() {
  @SDL_Quit();
}

U0 @SDL_DestroyMutex(SDL_mutex* mutex) {
  Free(mutex);
}
U0 SDL_DestroyMutex() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_DestroyMutex
  }
  POP_SYSV_REGS
}

U0 @SDL_DestroyCond(SDL_cond* cond) {
  Free(cond);
}
U0 SDL_DestroyCond() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_DestroyCond
  }
  POP_SYSV_REGS
}

I32 @SDL_GetCPUCount() {
  return mp_cnt;
}
U0 SDL_GetCPUCount() {
  @SDL_GetCPUCount();
}

I32 @SDL_SetRelativeMouseMode(Bool enabled) {
  return 0;
}
U0 SDL_SetRelativeMouseMode() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@SDL_SetRelativeMouseMode
  }
  POP_SYSV_REGS
}

U0 SDL_AudioThreadCaller() {
  PUSH_R2
//  PUSH_TOS_R
  asm {
    PUSH RBP
    MOV RBP,RSP

    MOV RCX,RSP
    AND ECX,15
    JE @@audio_init_aligned
    MOV EDX,16
    SUB EDX,ECX
    SUB RSP,RDX
@@audio_init_aligned:
    MOV EDX,U32 [&g_aud.buf_len]
    MOV RSI,[&g_aud.buf]
    MOV RDI,[&g_aud.userdata]
    CALL U64 [&g_aud.callback]
    MOV RSP,RBP
    POP RBP
  }
  POP_R2
//  POP_TOS_R
}

U0 SDL_AudioThread(SDL_AudioSpec *spec) {
  EnableSSE;
  SDL_AudioThreadData *aud = &g_aud;
  aud->callback = spec->callback;
  aud->userdata = spec->userdata;
  aud->pos = 0;
  aud->quit = 0;
  aud->freq = spec->freq;
  F64 ideal_delay = 1.0/aud->freq;
  aud->buf_len = spec->samples;
  aud->buf = MAllocAligned(sizeof(I16)*aud->buf_len, 16);
#ifdef DEBUG
  CommPrint("[TOS] [SDL_AudioThread] spec:%x\n", spec);
#endif
  SDL_AudioThreadCaller();

  /* QEMU freaks out if we just hammer on the PC speaker, so we track the state interally */
  /* turn off PC speaker to ensure consistent starting state */
  U32 tmp = InU8(0x61);
  OutU8(0x61, tmp & 0xFC);
  I32 spkr = 0;

  while (!aud->quit) {
    F64 start_t = tS;
    I16 val = aud->buf[aud->pos];

    if (val > 0x0) {
      spkr = 1;
#define PIT_OSC 1193180
      U64 div = PIT_OSC / val;
      OutU8(0x43, 0xb6);
      OutU8(0x42, div);
      OutU8(0x42, div>>8);
      tmp = InU8(0x61);
      if (tmp&3 == 0) {
        tmp = tmp|3;
        OutU8(0x61, tmp);
      }
    } else if (spkr) {
      spkr = 0;
      // Turn Off
      tmp = InU8(0x61);
      OutU8(0x61, tmp & 0xFC);
    }

    aud->pos++;
    if (aud->pos >= aud->buf_len) {
      aud->pos = 0;
      SDL_AudioThreadCaller();
    }

/*
//    U8 val = Round(ToF64(aud->buf[aud->pos])/255);
    I64 i, lag;
    F64 pitch = 0;
    F64 maxCorrelation = 0;
#define PITCH_SAMPLE_SIZE 128
#define SAMPLE_RATE 11025
    for (lag=1; lag<PITCH_SAMPLE_SIZE; lag++) {
      F64 correlation = 0;
      for (i=0; i<PITCH_SAMPLE_SIZE; i++) {
        correlation += aud->buf[i] * aud->buf[i+lag];
      }
      if (correlation > maxCorrelation) {
        maxCorrelation = correlation;
        pitch = ToF64(SAMPLE_RATE)/lag;
      }
    }
    if (pitch != 0)
      CommPrint("pitch:%f\n", pitch);
*/
//    @usleep(32);
    SleepUntil(cnts.jiffies);
    I64 end_t = (ideal_delay-(tS-start_t))*1000000;
    if (end_t > 0)
      Busy(end_t);
  }
}

U32 @SDL_OpenAudioDevice(U8 *device, I32 iscapture, SDL_AudioSpec *desired, SDL_AudioSpec *obtained, I32 allowed_changes) {
#ifdef DEBUG
  CommPrint("[TOS] [SDL_OpenAudioDevice]\n");
#endif
  //MemCpy(obtained, desired, sizeof(SDL_AudioSpec));
  //CJob* task = JobQue(&SDL_AudioThread, desired, mp_cnt-1, 0);
//  CTask* task = Spawn(&SDL_AudioThread, desired, "AudioThread", mp_cnt-1, Fs);
  CTask* task = Spawn(&SDL_AudioThread, desired, "AudioThread", mp_cnt-1);
  return 1;
}
U0 SDL_OpenAudioDevice() {
  PUSH_SYSV_REGS
  asm {
    PUSH R8
    PUSH RCX
    PUSH RDX
    PUSH RSI
    PUSH RDI
    CALL &@SDL_OpenAudioDevice
  }
  POP_SYSV_REGS
}

U0 @TOS_SetIRQ(I64 irq, U8* isr) {
  CommPrint("[TOS_SetIRQ] IRQ:%d ISR:%x\n", irq, isr);
  IntEntrySet(irq, isr);
}
U0 TOS_SetIRQ() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@TOS_SetIRQ
  }
  POP_SYSV_REGS
}

U8* @TOS_GetIRQ(I64 irq) {
  IntEntryGet(irq);
}
U0 TOS_GetIRQ() {
  PUSH_SYSV_REGS
  asm {
    PUSH RDI
    CALL &@TOS_GetIRQ
  }
  POP_SYSV_REGS
}
U64 TOS_GetTimer() {
  return IRQ_TIMER;
}

U0 @SDL_PauseAudioDevice(U32 dev, I32 pause_on) {
#ifdef DEBUG
  CommPrint("[TOS] [SDL_PauseAudioDvice] dev:%d pause_on:%d\n", dev, pause_on);
#endif
}
U0 SDL_PauseAudioDevice() {
  PUSH_SYSV_REGS
  asm {
    PUSH RSI
    PUSH RDI
    CALL &@SDL_PauseAudioDevice
  }
  POP_SYSV_REGS
}

U8* @SDL_GetError() {
  return "SDL_GetError is not implemented";
}
U0 SDL_GetError() {
  PUSH_SYSV_REGS
  asm {
    CALL &@SDL_GetError
  }
  POP_SYSV_REGS
}

U0 SDL_Delay() {}

#endif
