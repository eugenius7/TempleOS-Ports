//#define DEBUG

#include "Lib/CrunkComm";
#include "Lib/LibC";
#include "Lib/SDL2ScanCodes"
#include "Lib/SDL2";
#include "Lib/ELF64";

I32 quit_var = 0;

U0 BlazeItFgt() {
  InitComm;
  EnableSSE;

  SettingsPush;
  Suspend(sys_winmgr_task, TRUE);
  load_elf("libcyberchud.so");
  PUSH_R
  asm {
    PUSH RBP
    MOV RBP,RSP

    MOV RCX,RSP
    AND ECX,15
    JE init_aligned
    MOV EDX,16
    SUB EDX,ECX
    SUB RSP,RDX
init_aligned:
    MOV RDI,0
    CALL U64 [&_init]
    MOV RSP,RBP
    POP RBP
  }
  POP_R
  while (!quit_var) {
    PUSH_R
    asm {
      PUSH RBP
      MOV RBP,RSP
      MOV RCX,RSP
      AND ECX,15
      JE @@loop_aligned
      MOV EDX,16
      SUB EDX,ECX
      SUB RSP,RDX
@@loop_aligned:
      CALL U64 [&_loop]
      CALL U64 [&_check_if_quitting]
      MOV [&quit_var],EAX
      MOV RSP,RBP
      POP RBP
    }
    POP_R
  }
  PUSH_R
  asm {
    PUSH RBP
    MOV RBP,RSP
    MOV RCX,RSP
    AND ECX,15
    JE @@stack_aligned
    MOV EDX,16
    SUB EDX,ECX
    SUB RSP,RDX
@@stack_aligned:
    CALL U64 [&_quit]
    MOV RSP,RBP
    POP RBP
  }
  POP_R
  SndRst;
  Suspend(sys_winmgr_task, FALSE);
  SettingsPop;
  Refresh;
//  gr.fp_draw_ms=&DrawStdMs;
}
