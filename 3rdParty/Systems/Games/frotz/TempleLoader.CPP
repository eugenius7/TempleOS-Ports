// TempleOS loader for specially-built Linux programs
// See README

#define TL_MAX_HEAP_SIZE    0x1000000     // 16M - heap size
#define TL_MAX_FDS          10
#define TL_ALIGN_SIZE       0x1000

U64 * TL_Base;
U64 * TL_Argv;
U64 * TL_Restore_SP;
U64 TL_Argc;
U64 TL_Break_Start;
U64 TL_Break_End;
U64 TL_Program_Start;
U64 TL_Program_End;


class TL_FD
{
    CFile * f;
    U64 position;
    Bool write;
};

TL_FD TL_FD_Handle[TL_MAX_FDS];

class TL_ProgramHeader
{
    U64 start;
    U64 version;
    U64 syscall_address;
    U64 fini_address;
    U64 end_address;
    U64 load_size;
    U64 total_size;
    U8 pad[BLK_SIZE-(7*8)];
};

U0 TL_Restore_Point ()
{
    // The signature of this function must match TL_Launch
    asm {
        MOV     RCX,I64 [&TL_Restore_SP]
        MOV     RSP,RCX

        POP_REGS
        POP     RBP
    }
}

U8 * TL_To_Ptr (U64 p0)
{
    U8 * rc;
    MemCpy (&rc, &p0, 8);
    return rc;
}

U64 TL_To_Int (U8 * p0)
{
    U64 rc;
    MemCpy (&rc, &p0, 8);
    return rc;
}

U64 TL_Syscall_Ioctl (U64 p0, U64 p1, U64 p2)
{
    switch (p1) {
        case 0x5401:
            /* TCGETS */
            return 0;
        default:
            "\n\nunsupported ioctl reached, p0 = %d, p1 = %d, p2 = %d\n",
                p0, p1, p2;
            TL_Restore_Point;
            return 0;
    }
}

U0 TL_Read_Write_Common (Bool write, U64 fd, U8 * rbuf, U64 rsize)
{
    U64 blk_copy, blk_offset, i, num_blocks;
    U8 tmp[BLK_SIZE];

    num_blocks = (FSize (TL_FD_Handle[fd].f) + BLK_SIZE - 1) / BLK_SIZE;

    // for each block involved in this read/write:
    for (i = TL_FD_Handle[fd].position / BLK_SIZE; rsize > 0; i++) {

        if (i < num_blocks) {
            if (!FRBlks (TL_FD_Handle[fd].f, tmp, i, 1)) {
                "\n\nunable to read block %d from fd %d\n", i, fd;
                TL_Restore_Point;
            }
        } else {
            MemSet (tmp, 0, BLK_SIZE);
        }
        // where does the requested data begin within the block?
        blk_offset = TL_FD_Handle[fd].position - (i * BLK_SIZE);
        // how much data will be read within this block?
        blk_copy = rsize;
        if (rsize > (BLK_SIZE - blk_offset)) {
            blk_copy = BLK_SIZE - blk_offset;
        }

        if (write) {
            // copy from buffer to block (modifying the block)
            MemCpy (TL_To_Ptr (TL_To_Int (tmp) + blk_offset), rbuf, blk_copy);

            // write back
            if (!FWBlks (TL_FD_Handle[fd].f, tmp, i, 1)) {
                "\n\nunable to write block %d from fd %d\n", i, fd;
                TL_Restore_Point;
            }
        } else {
            // copy from block to buffer
            MemCpy (rbuf, TL_To_Ptr (TL_To_Int (tmp) + blk_offset), blk_copy);
        }
        // advance the counters
        TL_FD_Handle[fd].position += blk_copy;
        rsize -= blk_copy;
        rbuf = TL_To_Ptr (TL_To_Int (rbuf) + blk_copy);
    }
}


U64 TL_Syscall_Read (U64 fd, U8 * buf, U64 size)
{
    U64 remaining, total_size;

    if (size == 0) {
        return 0;
    }
    if (fd == 0) {
        // read \n-terminated string from stdin
        U64 x = GetS (buf, size);
        buf[x] = '\n';
        return x + 1;
    }
    // read from some other file handle (check the handle first)
    if ((fd >= TL_MAX_FDS) || (TL_FD_Handle[fd].f == NULL)) {
        "\n\nunsupported read from fd %d\n", fd;
        TL_Restore_Point;
    }
    // bound read by remaining file size
    total_size = FSize (TL_FD_Handle[fd].f);
    if (total_size >= TL_FD_Handle[fd].position) {
        remaining = total_size - TL_FD_Handle[fd].position;
    } else {
        remaining = 0;
    }
    if (size > remaining) {
        size = remaining;
    }

    TL_Read_Write_Common (FALSE, fd, buf, size);
    //"read %d bytes from fd %d to %p\n", size, fd, buf;
    return size;
}

U64 TL_Syscall_Write (U64 fd, U8 * buf, U64 size)
{
    U64 i;
    if ((fd == 0) || (fd >= TL_MAX_FDS)
    || ((fd > 2)
        && ((TL_FD_Handle[fd].f == NULL) || (!TL_FD_Handle[fd].write)))) {

        "\n\nunsupported write to fd %d\n", fd;
        TL_Restore_Point;
    }
    if (fd <= 2) {
        // write to stdout/stderr
        for (i = 0; i < size; i++) {
            if (buf[i] == 0x0a) {
                "\n";
            } else if (buf[i] != 0x0d) {
                '' buf[i];
            }
        }
        return size;
    }
    TL_Read_Write_Common (TRUE, fd, buf, size);
    return size;
}

U64 TL_Syscall_Open_FD (Bool write, CFile * f)
{
    I64 fd, i;

    if (!f) {
        // ENOENT, could not open file
        return -2;
    }
    fd = -24; // EMFILE
    for (i = 3; i < TL_MAX_FDS; i++) {
        if (TL_FD_Handle[i].f == NULL) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        FClose (f);
        return fd; // EMFILE, too many open files
    }
    TL_FD_Handle[fd].position = 0;
    TL_FD_Handle[fd].write = write;
    TL_FD_Handle[fd].f = f;

    //"open '%s' -> fd %d\n", buf, fd;
    return fd;
}

U64 TL_Syscall_Open (U8 * buf, U64 mode)
{
    CFile * f;
    switch (mode & 3) {
        case 0x0:
            // read mode
            return TL_Syscall_Open_FD (FALSE, FOpen (buf, "r"));
        case 0x1:
            // write mode
            switch (mode) {
                case 0x241:
                    // write with O_CREAT and O_TRUNC flags
                    f = FOpen (buf, "r");
                    if (f) {
                        // file exists, delete it
                        FClose (f);
                        Del (buf);
                    }
                    return TL_Syscall_Open_FD (TRUE, FOpen (buf, "w+"));
                default:
                    break;
            }
        default:
            break;
    }
    "\n\nunsupported open mode for file '%s': %d\n", buf, mode;
    TL_Restore_Point;
}

U64 TL_Syscall_Close (U64 fd)
{
    if ((fd >= TL_MAX_FDS) || (fd <= 2) || (TL_FD_Handle[fd].f == NULL)) {
        "\n\nunsupported close for fd %d\n", fd;
        TL_Restore_Point;
    }
    FClose (TL_FD_Handle[fd].f);
    TL_FD_Handle[fd].f = NULL;
    TL_FD_Handle[fd].position = 0;
    return 0;
}

U64 TL_Syscall_LSeek (U64 fd, I64 offset, U64 whence)
{
    if ((fd >= TL_MAX_FDS) || (fd <= 2) || (TL_FD_Handle[fd].f == NULL)) {
        "\n\nunsupported lseek for fd %d\n", fd;
        TL_Restore_Point;
    }
    switch (whence) {
        case 0:
            // SEEK_SET
            break;
        case 1:
            // SEEK_CUR
            offset += TL_FD_Handle[fd].position;
            break;
        case 2:
            // SEEK_END
            offset += FSize (TL_FD_Handle[fd].f);
            break;
        default:
            "\n\nunsupported lseek whence %d for fd %d\n", whence, fd;
            TL_Restore_Point;
    }
    if ((offset < 0) || (offset > FSize (TL_FD_Handle[fd].f))) {
        // return EINVAL
        return -22;
    }
    TL_FD_Handle[fd].position = offset;
    return offset;
}

U64 TL_Syscall_FStat (U64 fd, U64 * stat)
{
    if ((fd >= TL_MAX_FDS) || (fd <= 2) || (TL_FD_Handle[fd].f == NULL)) {
        "\n\nunsupported fstat for fd %d\n", fd;
        TL_Restore_Point;
    }
    // only set the overall file size, everything else is zero
    MemSet (stat, 0, 144);
    stat[6] = FSize (TL_FD_Handle[fd].f);
    return 0;
}

U64 TL_Syscall_Brk (U64 pos)
{
    // brk (allocate memory)
    if (pos < TL_Break_Start) {
        pos = TL_Break_Start;
    }
    if (pos > TL_Break_End) {
        pos = TL_Break_End;
    }
    return pos;
}

U64 TL_GetTimeOfDay (U64 * timeval, U64 * timezone)
{
    if (timeval) {
        MemSet (timeval, 0, 16);
        timeval[0] = 1400000000;
    }
    if (timezone) {
        MemSet (timezone, 0, 8);
    }
    return 0;
}

U0 TL_Register_Dump (U64 * rax)
{
    U64 i, j, p;
    j = 0;
    for (i = 15; i >= 8; i--) {
        "r%d = %08x\n", i, rax[j];
        j++;
    }
    "rdi = %08x\n", rax[j]; j++;
    "rsi = %08x\n", rax[j]; j++;
    "rbp = %08x\n", rax[j]; j++;
    "rbx = %08x\n", rax[j]; j++;
    "rdx = %08x\n", rax[j]; j++;
    "rcx = %08x\n", rax[j]; j++;
    "rax = %08x\n", rax[j]; j++;
    "return %08x -> %08x\n", rax[j], rax[j] - TL_Program_Start;
    "rsp %p\n", &rax[j]; j++;
    for (i = 0; i < 2048; i++) {
        p = rax[j]; j++;
        if ((p > TL_Program_Start) && (p < TL_Program_End)) {
            "above[%d] = %08x -> %08x\n", i, p, p - TL_Program_Start;
        }
    }
}

U64 TL_Syscall(U64 number, U64 p0, U64 p1, U64 p2, U64 * rax)
{
    switch (number) {
        case 0:
            return TL_Syscall_Read (p0, TL_To_Ptr (p1), p2);
        case 1:
            return TL_Syscall_Write (p0, TL_To_Ptr (p1), p2);
        case 2:
            return TL_Syscall_Open (TL_To_Ptr (p0), p1);
        case 3:
            return TL_Syscall_Close (p0);
        case 5:
            return TL_Syscall_FStat (p0, TL_To_Ptr (p1));
        case 8:
            return TL_Syscall_LSeek (p0, p1, p2);
        case 12:
            return TL_Syscall_Brk (p0);
        case 16:
            return TL_Syscall_Ioctl (p0, p1, p2);
        case 60:
            //"\n\nexit\n";
            TL_Restore_Point;
            return 0;
        case 201:
            // time
            return 1400000000;
        case 96:
            return TL_GetTimeOfDay (p0, p1);
        default:
            "\n\nunsupported system call reached! number = %d\n", number;
            "p0 = %x (%d), p1 = %x (%d), p2 = %x (%d)\n",
                p0, p0, p1, p1, p2, p2;
            TL_Register_Dump (rax);
            TL_Restore_Point;
            return 0;
    }
    return 0;
}

U0 TL_Launch ()
{
    // The signature of this function must match TL_Restore_Point
    asm {
        PUSH    RBP
        MOV     RBP, RSP

        PUSH_REGS

        MOV     RCX,RSP
        MOV     I64 [&TL_Restore_SP], RCX
        MOV     RDI,I64 [&TL_Argc]
        MOV     RSI,I64 [&TL_Argv]
        MOV     RCX,I64 [&TL_Base]

        CALL    RCX

        HLT
    }
}

U0 TL_Syscall_Point ()
{
    // This function is called from the Linux program
    asm {
        PUSH_REGS
        // TempleOS: push args on stack in right-to-left order
        //           Called function cleans up the stack
        // Linux: args in registers (l->r): RDI, RSI, RDX, RCX
        // Both: returned value in RAX
       
        MOV     RAX, RSP
        PUSH    RAX
        PUSH    RCX
        PUSH    RDX
        PUSH    RSI
        PUSH    RDI
        CALL    &TL_Syscall

        // returned value will be restored by POP_REGS:
        // RSP[0*8] is r15
        // RSP[1*8] is r14
        // ...
        // RSP[14*8] is rax
        // 14*8 = 112
        MOV     I64 [RSP+112],RAX
        POP_REGS
    }
}

Bool TL_Loader(...)
{
    TL_ProgramHeader header;
    CFile *f;
    U64 num_blocks;
    U64 offset;
    U8 * base_copy;
    U8 * true_base;
    U8 * table_entry;
    U8 * file_entry;
    U8 * program;
    U64 i, tmp;
    U32 reloc;

    if (argc < 1) {
        "Usage: TL_Loader (\"program.prg\", ...);\n\n";
        return FALSE;
    }
    program = argv[0];

    // read first block
    f = FOpen (program, "r");
    if (!f) {
        "Unable to open '%s' file\n\n", program;
        return FALSE;
    }
    if (!FRBlks (f, &header, 0, 1)) {
        "Unable to read header from '%s' file\n\n", program;
        return FALSE;
    }
    //"program header:\n";
    //"fini_address %d.\n", header.fini_address;
    //"end_address %d.\n", header.end_address;
    //"load_size %d.\n", header.load_size;
    //"total_size %d.\n", header.total_size;

    if ((header.start >> 32) != 0x77646a00) {
        FClose (f);
        "Program file '%s' is not valid.\n", program;
        return FALSE;
    }

    num_blocks = (header.total_size + BLK_SIZE - 1) / BLK_SIZE;

    //"number of blocks %d.\n", num_blocks;

    // allocate
    true_base = MAlloc (TL_MAX_HEAP_SIZE + header.total_size + TL_ALIGN_SIZE);
    MemSet (true_base, 0x0,
            TL_MAX_HEAP_SIZE + header.total_size + TL_ALIGN_SIZE);

    // round up to page
    offset = TL_To_Int (true_base);
    offset = (offset + TL_ALIGN_SIZE - 1) / TL_ALIGN_SIZE;
    offset *= TL_ALIGN_SIZE;
    TL_Base = TL_To_Ptr (offset);
    base_copy = TL_To_Ptr (offset);

    //"base %p (aligned from %p)\n", TL_Base, true_base;

    for (i = 0; i < num_blocks; i++) {
        file_entry = base_copy + (i * BLK_SIZE);
        if (!FRBlks (f, file_entry, i, 1)) {
            "Unable to read content from '%s' file\n\n", program;
            return FALSE;
        }
    }
    FClose (f);
    //"loaded blocks\n";

    // read the relocation table and do the relocations..
    for (i = header.load_size; i < header.total_size; i += 4) {
        table_entry = base_copy + i;
        MemCpy (&reloc, table_entry, 4);
        if (reloc >= (header.load_size / 8)) {
           "relocation %d invalid.\n", reloc;
           return FALSE;
        }
        reloc = reloc * 8;
        MemCpy (&tmp, base_copy + reloc, 8);
        tmp += offset;
        MemCpy (base_copy + reloc, &tmp, 8);
    }
    //"relocated blocks\n";

    // Zero area occupied by relocations
    // (It's also the BSS area)
    table_entry = base_copy + header.load_size;
    MemSet (table_entry, 0, header.total_size - header.load_size);

    // Heap space:
    TL_Break_Start = header.end_address + offset;
    TL_Break_End = header.total_size + TL_MAX_HEAP_SIZE + offset;

    // Program space
    TL_Program_Start = offset;
    TL_Program_End = offset + header.fini_address;

    // Setup environment.
    // Args are copied from built-in variables argc and argv
    TL_Base[2] = &TL_Syscall_Point;
    TL_Argv = MAlloc ((argc * 8) + 64);
    MemSet (TL_Argv, 0x00, (argc * 8) + 64);
    TL_Argc = argc;
    for (i = 0; i < argc; i++) {
        TL_Argv[i] = TL_To_Ptr (argv[i]);
    }
    for (i = 0; i < TL_MAX_FDS; i++) {
        TL_FD_Handle[i].f = NULL;
    }

    //"heap start %p.\n", TL_To_Ptr (TL_Break_Start);
    //"heap end %p.\n", TL_To_Ptr (TL_Break_End);

    //"launching\n";
    TL_Launch;
    //"\nreturned from program\n";
    Free (TL_Argv);
    Free (true_base);
    for (i = 0; i < TL_MAX_FDS; i++) {
        if (TL_FD_Handle[i].f != NULL) {
            TL_Syscall_Close (i);
        }
    }
    //"memory freed\n";
    return TRUE;
}

