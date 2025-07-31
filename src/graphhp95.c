#include <dos.h>
#include "graphhp95.h"

/*
  GraphInit: set 80×25 text mode (mode 3) as placeholder
  HP95LX’s “quarter-CGA” can be emulated later if you grab the real SDK.
*/
void GraphInit(void) {
    union REGS regs;
    regs.h.ah = 0x00;   /* BIOS Set Video Mode */
    regs.h.al = 0x03;   /* Mode 3 = 80×25 color text */
    int86(0x10, &regs, &regs);
}

/*
  GraphShutdown: simply restore mode 3 again
  (if you’d switched to any other mode, this brings you back to text).
*/
void GraphShutdown(void) {
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
}
