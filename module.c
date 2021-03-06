/******************************************************************************\
* Project:  Module Subsystem Interface to SP Interpreter Core                  *
* Authors:  Iconoclast                                                         *
* Release:  2018.12.18                                                         *
* License:  CC0 Public Domain Dedication                                       *
*                                                                              *
* To the extent possible under law, the author(s) have dedicated all copyright *
* and related and neighboring rights to this software to the public domain     *
* worldwide. This software is distributed without any warranty.                *
*                                                                              *
* You should have received a copy of the CC0 Public Domain Dedication along    *
* with this software.                                                          *
* If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.             *
\******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* time() (for helping srand()) */
#include <time.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "module.h"
#include "su.h"

#include <signal.h>
#include <setjmp.h>

static jmp_buf CPU_state;
static void seg_av_handler(int signal_code)
{
    longjmp(CPU_state, signal_code);
}
static void ISA_op_illegal(int signal_code)
{
    message("Plugin built for SIMD extensions this CPU does not support!");
    raise(signal_code); /* e.g., rsp.dll built with -mssse3; the CPU is SSE2. */
}

RSP_INFO RSP_INFO_NAME;
static const char DLL_about[] =
    "RSP Interpreter by Iconoclast\n"\
    "Thanks for test RDP:  Jabo, ziggy, angrylion\n"\
    "RSP driver examples:  bpoint, zilmar, Ville Linde";

static void init_regs(void)
{
    register size_t i, j;

    for (i = 0; i < 16; i++)
        if (CR[i] == NULL)
            raise(SIGTERM); /* Don't proceed if plugin hasn't initialized. */
    srand(time(NULL));

    for (i = 0; i < N; i++) {
        VACC_H[i] = ((u64)0xFFFF00000000 >> 32) & 0x0000;
        VACC_M[i] = ((u64)0x0000FFFF0000 >> 16) & 0x0000;
        VACC_L[i] = ((u64)0x00000000FFFF >>  0) & 0x0000;
    }
#if 0
    DPH = SP_DIV_PRECISION_SINGLE; /* static global maintained in vu/divide.o */
#endif

/*
 * Based on krom's experiences at testing the RSP hardware with homebrew, it
 * has become apparent that the bits in $vco, $vcc and $vce do NOT become
 * random upon powering on the console.  However, this does not say that
 * previous values of these flags aren't momentarily preserved before any
 * bit rot loses them overtime.  Since it's not clear whether these flags are
 * explicitly initialized to 0 at power-on or if they temporarily retain old
 * decaying bits, we'll just make them 0 to hush krom's RSP test FAIL yells.
 */
    for (i = 0; i < N; i++) {
        cf_ne[i]   = (rand() & (1 << 15)) ? 0*TRUE : FALSE;
        cf_co[i]   = (rand() & (1 << 12)) ? 0*TRUE : FALSE;
        cf_clip[i] = (rand() & (1 <<  9)) ? 0*TRUE : FALSE;
        cf_comp[i] = (rand() & (1 <<  6)) ? 0*TRUE : FALSE;

        cf_vce[i]  = (rand() & (1 <<  0)) ? 0*TRUE : FALSE;
    }

    for (i = 0; i < 32; i++)
        SR[i] = (u32)rand();
    SR[0] = 0x00000000;
    for (i = 0; i < 32; i++)
        for (j = 0; j < N; j++)
            VR[i][j] = (u16)((u32)rand() & 0xFFFFu);

    *(RSP_INFO_NAME.SP_PC_REG) = REGVAL(0x04001000u);

    *CR[0x0] = 0x00000000; /* DMA transfer address for SP memory cache */
    *CR[0x1] = 0x00000000; /* DMA transfer address for host DRAM */
    *CR[0x2] = 0x00000000; /* DMA read transfer period */
    *CR[0x3] = 0x00000000; /* DMA write transfer period */

    *CR[0x4] = REGVAL(0x00000001u); /* SP status flags */
    *CR[0x5] = 0x00000000; /* read-only DMA full indicator */
    *CR[0x6] = 0x00000000; /* read-only DMA busy indicator */
    *CR[0x7] = 0x00000000; /* CPU-RSP synchronicity semaphore */

    *CR[0x8] = REGVAL((u32)rand()); /* start address of RDP command buffer */
    *CR[0x9] = REGVAL((u32)rand()); /* end address of RDP command buffer */
    *CR[0xA] = 0x00000000; /* read-only current RDP command buffer address */
    *CR[0xB] &=     REGVAL(0x100u); /* DP status flags:  DMA_BUSY flag is undefined. */

    *CR[0xC] = REGVAL(0x0000FFFFu); /* RDP clock cycle counter */
/*
 * Technically these are random at startup on the hardware, but most emulators
 * fail to ever clear these locks if randomly set, causing constant warnings.
 */
#if 0
    *CR[0xD] = (u32)rand(); /* read-only RDP contiguous busy buffer cycles */
    *CR[0xE] = (u32)rand(); /* read-only RDP contiguous busy pipe cycles */
#endif
    *CR[0xF] = REGVAL((u32)rand()); /* read-only RDP contiguous TMEM import cycles */

    // *CR[0xB] |= REGVAL(0x000000A8u); /* GCLK, PIPE_BUSY and CMD_BUF_READY always set */
#if 0
    *CR[0xB] |= (irand() & 1) << 8; /* DP DMA busy status bit is undefined. */
#endif
    *CR[0xC] += REGVAL(*CR[0xD]) + REGVAL(*CR[0xE]) + REGVAL(*CR[0xF]); /* random total clock cycles */
}

EXPORT void CALL CloseDLL(void)
{
    DRAM = NULL; /* so DllTest benchmark doesn't think ROM is still open */
    return;
}

EXPORT void CALL DllAbout(p_void hParent)
{
    message(DLL_about);
    hParent = NULL;
    if (hParent == NULL)
        return; /* -Wunused-but-set-parameter */
    return;
}

EXPORT void CALL DllConfig(p_void hParent)
{
    int result = system("sp_cfgui");
    update_conf(CFG_FILE);

    if (DMEM == IMEM || REGVAL(GET_RCP_REG(SP_PC_REG)) % 4096 == 0x00000000)
        return;
    export_SP_memory();

    hParent = NULL;
    if (hParent == NULL)
        return; /* -Wunused-but-set-parameter */
    return;
}

EXPORT u32 CALL DoRspCycles(u32 cycles)
{
    static char task_debug[] = "unknown task type:  0x????????";
    char* task_debug_type;
    OSTask_type task_type;
    register unsigned int i;

    if (REGVAL(GET_RCP_REG(SP_STATUS_REG)) & 0x00000003) {
        // message("SP_STATUS_HALT");
        return 0x00000000;
    }

    task_debug_type = &task_debug[strlen("unknown task type:  0x")];

#if (DIRECT_MEMORY == 1)
    memcpy(&task_type, DMEM + 0xFC0, 4);
#else
    task_type = 0x00000000
      | (u32)(DMEM[0xFC0 ^ 0] & 0xFFu) << 24
      | (u32)(DMEM[0xFC1 ^ 0] & 0xFFu) << 16
      | (u32)(DMEM[0xFC2 ^ 0] & 0xFFu) <<  8
      | (u32)(DMEM[0xFC3 ^ 0] & 0xFFu) <<  0
    ;
#endif
    switch (task_type) {
        
    case M_GFXTASK:
        if (CFG_HLE_GFX == 0)
            break;

        if (MEM32(*(pi32)(DMEM + 0xFF0)) == 0x00000000)
            break; /* Resident Evil 2, null task pointers */
        if (GET_RSP_INFO(ProcessDList) == NULL)
            { /* branch */ }
        else
            GET_RSP_INFO(ProcessDList)();

        GET_RCP_REG(SP_STATUS_REG) |=
            REGVAL(SP_STATUS_SIG2 | SP_STATUS_BROKE | SP_STATUS_HALT);

        if (GET_RCP_REG(SP_STATUS_REG) & REGVAL(SP_STATUS_INTR_BREAK)) {
            GET_RCP_REG(MI_INTR_REG) |= REGVAL(0x00000001);
            GET_RSP_INFO(CheckInterrupts)();
        }
        GET_RCP_REG(DPC_STATUS_REG) &= REGVAL(~0x00000002ul); /* DPC_STATUS_FREEZE */
        return 0;
    case M_AUDTASK:
        if (CFG_HLE_AUD == 0)
            break;

        if (GET_RSP_INFO(ProcessAList) == NULL)
            { /* branch */ }
        else
            GET_RSP_INFO(ProcessAList)();

        GET_RCP_REG(SP_STATUS_REG) |=
            REGVAL(SP_STATUS_SIG2 | SP_STATUS_BROKE | SP_STATUS_HALT)
        ;
        if (GET_RCP_REG(SP_STATUS_REG) & REGVAL(SP_STATUS_INTR_BREAK)) {
            GET_RCP_REG(MI_INTR_REG) |= REGVAL(0x00000001);
            GET_RSP_INFO(CheckInterrupts)();
        }
        return 0;
    case M_VIDTASK:
        message("M_VIDTASK");
        break;
    case M_NJPEGTASK:
        break; /* Zelda, Pokemon, others */
    case M_NULTASK:
        message("M_NULTASK");
        break;
    case M_HVQTASK:
        message("M_HVQTASK");
        break;
    case M_HVQMTASK:
        if (GET_RSP_INFO(ShowCFB) == NULL) /* Gfx #1.2 or older specs */
            break;
        GET_RSP_INFO(ShowCFB)(); /* forced FB refresh in case gfx plugin skip */
        break;
    default:
        if (task_type == 0x00000000)
            break; /* generic or invoked without CPU filling in OSTask struct */
        if (task_type == 0x8BC43B5D)
            break; /* CIC boot code sent to the RSP */
        sprintf(task_debug_type, "%08lX", (unsigned long)task_type);
        message(task_debug);
    }

#ifdef WAIT_FOR_CPU_HOST
    for (i = 0; i < NUMBER_OF_SCALAR_REGISTERS; i++)
        MFC0_count[i] = 0;
#endif
    run_task();

/*
 * An optional EMMS when compiling with Intel SIMD or MMX support.
 *
 * Whether or not MMX has been executed in this emulator, here is a good time
 * to finally empty the MM state, at the end of a long interpreter loop.
 */
#ifdef ARCH_MIN_SSE2
    _mm_empty();
#endif

    if (*CR[0x4] & REGVAL(SP_STATUS_BROKE)) /* normal exit, from executing BREAK */
        return (cycles);
    else if (GET_RCP_REG(MI_INTR_REG) & REGVAL(1)) /* interrupt set by MTC0 to break */
        GET_RSP_INFO(CheckInterrupts)();
    else if (*CR[0x7] != 0x00000000) /* semaphore lock fixes */
        {}
#ifdef WAIT_FOR_CPU_HOST
    else
        MF_SP_STATUS_TIMEOUT = 16; /* From now on, wait 16 times, not 32767. */
#else
    else { /* ??? unknown, possibly external intervention from CPU memory map */
        message("SP_SET_HALT");
        return (cycles);
    }
#endif
    *CR[0x4] &= REGVAL(~SP_STATUS_HALT); /* CPU restarts with the correct SIGs. */
    return (cycles);
}

EXPORT void CALL GetDllInfo(PLUGIN_INFO *PluginInfo)
{
    PluginInfo -> Version = PLUGIN_API_VERSION;
    PluginInfo -> Type = PLUGIN_TYPE_RSP;
    strcpy(PluginInfo -> Name, "Static Interpreter");
    PluginInfo -> NormalMemory = 0;
    PluginInfo -> MemoryBswaped = DIRECT_MEMORY;
    return;
}

p_func GBI_phase;
void no_LLE(void)
{
    static int already_warned;

    if (already_warned)
        return;
    message("RSP configured for LLE but not using LLE graphics plugin.");
    already_warned = TRUE;
    return;
}
EXPORT void CALL InitiateRSP(RSP_INFO Rsp_Info, pu32 CycleCount)
{
    int recovered_from_exception;

    if (CycleCount != NULL) /* cycle-accuracy not doable with today's hosts */
        *CycleCount = 0;
    update_conf(CFG_FILE);

    RSP_INFO_NAME = Rsp_Info;
    DRAM = GET_RSP_INFO(RDRAM);
    if (Rsp_Info.DMEM == Rsp_Info.IMEM) /* usually dummy RSP data for testing */
        return; /* DMA is not executed just because plugin initiates. */
    DMEM = GET_RSP_INFO(DMEM);
    IMEM = GET_RSP_INFO(IMEM);

    CR[0x0] = &GET_RCP_REG(SP_MEM_ADDR_REG);
    CR[0x1] = &GET_RCP_REG(SP_DRAM_ADDR_REG);
    CR[0x2] = &GET_RCP_REG(SP_RD_LEN_REG);
    CR[0x3] = &GET_RCP_REG(SP_WR_LEN_REG);
    CR[0x4] = &GET_RCP_REG(SP_STATUS_REG);
    CR[0x5] = &GET_RCP_REG(SP_DMA_FULL_REG);
    CR[0x6] = &GET_RCP_REG(SP_DMA_BUSY_REG);
    CR[0x7] = &GET_RCP_REG(SP_SEMAPHORE_REG);
    CR[0x8] = &GET_RCP_REG(DPC_START_REG);
    CR[0x9] = &GET_RCP_REG(DPC_END_REG);
    CR[0xA] = &GET_RCP_REG(DPC_CURRENT_REG);
    CR[0xB] = &GET_RCP_REG(DPC_STATUS_REG);
    CR[0xC] = &GET_RCP_REG(DPC_CLOCK_REG);
    CR[0xD] = &GET_RCP_REG(DPC_BUFBUSY_REG);
    CR[0xE] = &GET_RCP_REG(DPC_PIPEBUSY_REG);
    CR[0xF] = &GET_RCP_REG(DPC_TMEM_REG);
    init_regs();

    MF_SP_STATUS_TIMEOUT = 32767;
#if 1
    GET_RCP_REG(SP_PC_REG) &= REGVAL(0x00000FFFu); /* hack to fix Mupen64 */
#endif

    GBI_phase = GET_RSP_INFO(ProcessRdpList);
    if (GBI_phase == NULL)
        GBI_phase = no_LLE;

    signal(SIGILL, ISA_op_illegal);
#ifndef _WIN32
    signal(SIGSEGV, seg_av_handler);
    for (SR[ra] = 0; SR[ra] < 0x80000000ul; SR[ra] += 0x200000) {
        recovered_from_exception = setjmp(CPU_state);
        if (recovered_from_exception)
            break;
        SR[at] += DRAM[SR[ra]];
    }
    for (SR[at] = 0; SR[at] < 31; SR[at]++) {
        SR[ra] = (SR[ra] & ~1) >> 1;
        if (SR[ra] == 0)
            break;
    }
    su_max_address = (1 << SR[at]) - 1;
#endif

    if (su_max_address < 0x1FFFFFul)
        su_max_address = 0x1FFFFFul; /* 2 MiB */
    if (su_max_address > 0xFFFFFFul)
        su_max_address = 0xFFFFFFul; /* 16 MiB */
    return;
}

EXPORT void CALL RomClosed(void)
{
    FILE* stream;

    GET_RCP_REG(SP_PC_REG) = REGVAL(0x04001000u);

/*
 * Sometimes the end user won't correctly install to the right directory. :(
 * If the config file wasn't installed correctly, politely shut errors up.
 */
    stream = fopen(CFG_FILE, "wb");
    fwrite(conf, 8, 32 / 8, stream);
    fclose(stream);
    return;
}


EXPORT void CALL SetInstDebugHandler(inst_debug_func callback) {
    callback_inst_debug = callback;
}

EXPORT SHARE_STATE CALL GetCoreState() {
    SHARE_STATE state;

    state.VectorRegisters = get_ptr_VR();
    state.Accumulator = get_ptr_VACC();
    state.SURegisters = (pu8)&SR;

    return state;
}


EXPORT void CALL SingleStep() {
    single_step();
}

EXPORT u16 CALL GetVc0() {
    return get_VCO();
}

EXPORT u16 CALL GetVcc() {
    return get_VCC();
}

EXPORT u8 CALL GetVce() {
    return get_VCE();
}

EXPORT void CALL SetVc0(u16 value) {
    set_VCO(value);
}

EXPORT void CALL SetVcc(u16 value) {
    set_VCC(value);
}

EXPORT void CALL SetVce(u8 value) {
    set_VCE(value);
}

EXPORT void CALL DmaRead() {
    SP_DMA_READ();
}

EXPORT void CALL DmaWrite() {
    SP_DMA_WRITE();
}

NOINLINE void message(const char* body)
{
#ifdef WIN32
    char* argv;
    int i, j;

    argv = calloc(strlen(body) + 64, 1);
    strcpy(argv, "CMD /Q /D /C \"TITLE RSP Message&&ECHO ");
    i = 0;
    j = strlen(argv);
    while (body[i] != '\0') {
        if (body[i] == '\n') {
            strcat(argv, "&&ECHO ");
            ++i;
            j += 7;
            continue;
        }
        argv[j++] = body[i++];
    }
    strcat(argv, "&&PAUSE&&EXIT\"");
    system(argv);
    free(argv);
#else
    printf("RSP PLUGIN: %s\n", body);
#endif
    return;
}

NOINLINE void update_conf(const char* source)
{
    FILE* stream;
    register int i;

/*
 * hazard adjustment
 * If file not found, wipe the registry to 0's (all default settings).
 */
    for (i = 0; i < 32; i++)
        conf[i] = 0x00;

    stream = fopen(source, "rb");
    if (stream == NULL) {
        message("Failed to read config.");
        return;
    }
    int result = fread(conf, 8, 32 / 8, stream);
    fclose(stream);
    return;
}

#ifdef SP_EXECUTE_LOG
void step_SP_commands(uint32_t inst)
{
    unsigned char endian_swap[4];
    char text[256];
    char offset[4] = "";
    char code[9] = "";

    if (output_log == NULL)
        return;

    endian_swap[00] = (u8)((inst >> 24) & 0xFF);
    endian_swap[01] = (u8)((inst >> 16) & 0xFF);
    endian_swap[02] = (u8)((inst >>  8) & 0xFF);
    endian_swap[03] = (u8)((inst >>  0) & 0xFF);
    sprintf(&offset[0], "%03X", GET_RCP_REG(SP_PC_REG) & 0xFFF);
    sprintf(&code[0], "%08X", inst);
    strcpy(text, offset);
    strcat(text, "\n");
    strcat(text, code);
    message(text); /* PC offset, MIPS hex. */
    if (output_log != NULL)
        fwrite(endian_swap, 4, 1, output_log);
}
#endif

NOINLINE void export_data_cache(void)
{
    pu8 DMEM_swapped;
    FILE * out;
    register int i;
 /* const int little_endian = GET_RSP_INFO(MemoryBswaped); */

    DMEM_swapped = calloc(4096, 1);

    for (i = 0; i < 4096; i++)
        #ifndef VIRTUAL_BE
        DMEM_swapped[i] = DMEM[BES(i)];
        #else
        DMEM_swapped[i] = DMEM[i];
        #endif

    out = fopen("rcpcache.dhex", "wb");
    fwrite(DMEM_swapped, 16, 4096 / 16, out);
    fclose(out);
    free(DMEM_swapped);
    return;
}
NOINLINE void export_instruction_cache(void)
{
    pu8 IMEM_swapped;
    FILE * out;
    register int i;
 /* const int little_endian = GET_RSP_INFO(MemoryBswaped); */

    IMEM_swapped = calloc(4096, 1);

    for (i = 0; i < 4096; i++)
        #ifndef VIRTUAL_BE
        IMEM_swapped[i] = IMEM[BES(i)];
        #else
        IMEM_swapped[i] = IMEM[i];
        #endif

    out = fopen("rcpcache.ihex", "wb");
    fwrite(IMEM_swapped, 16, 4096 / 16, out);
    fclose(out);
    free(IMEM_swapped);
    return;
}
void export_SP_memory(void)
{
    export_data_cache();
    export_instruction_cache();
    return;
}

/*
 * Microsoft linker defaults to an entry point of `_DllMainCRTStartup',
 * which attaches several CRT dependencies.  To eliminate linkage of unused
 * startup CRT code, we direct the linker to use DllMain as the entry point.
 *
 * The same approach is taken with MinGW to get those weird MinGW-specific
 * messages and unused initializer functions out of the plugin binary.
 */
#ifdef _WIN32
BOOL WINAPI
DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    hModule = lpReserved = NULL; /* unused */
    switch (ul_reason_for_call) {
    case 1:  /* DLL_PROCESS_ATTACH */
    case 2:  /* DLL_THREAD_ATTACH */
    case 3:  /* DLL_THREAD_DETACH */
    case 0:  /* DLL_PROCESS_DETACH */
        break;
    default:
        message("Unknown reason for call.");
    }
    return TRUE;
}
#endif
