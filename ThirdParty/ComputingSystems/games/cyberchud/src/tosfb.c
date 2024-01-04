#include "tosfb.h"
#include "bitarr.h"
#include "mytime.h"
#include "text.h"
#include "stb_sprintf.h"

extern void* TOS_GetIRQ(u64 irq);
extern void TOS_SetIRQ(u64 irq, void* isr);

#define JIFFY_FREQ 1000 // Hz
#define SYS_TIMER0_PERIOD (65536*182/10/JIFFY_FREQ)

typedef struct tos_ccpu_s {
	struct tos_ccpu_s* addr;
	i64 num;
	i64 cpu_flags;
	i64 startup_rip;
	i64 idle_pt_hits;
	double idle_factor;
	i64 total_jiffies;
	void* seth_task;
	void* idle_task;
	i64 tr;
	i64 swap_cnter;
	void* profiler_timer_irq;
	void* next_dying;
	void* last_dying;
	i64 kill_jiffy;
	void* tss;
	i64 start_stk[16];
} tos_ccpu_t;

typedef struct {
	u64 locked_flags;
	u64 alloced_u8s;
	u64 used_u8s;
#if 0
#define MEM_PAG_BITS 9
#define MEM_FREE_PAG_HASH_SIZE 0x100
	void* mem_free_lst;
	void* mem_free_2meg_lst; //This is for Sup1CodeScraps/Mem/Mem2Meg.HC.
	void* free_pag_hash[MEM_FREE_PAG_HASH_SIZE];
	void* free_pag_hash2[64-MEM_PAG_BITS];
#endif
} tos_cblkpool_t;

typedef struct {
	const volatile u64* hpet_addr;
	uint64_t hpet_initial;
	double hpet_freq;
} tos_time_t;

typedef struct {
	const volatile tos_ccpu_t* ccpu;
	tos_time_t hpet;
	u8* vga_mem;
	const volatile tos_cblkpool_t* bp_code;
	const volatile tos_cblkpool_t* bp_data;
} tos_data_t;

extern void get_tos_data(tos_data_t*);
static tos_data_t g_tos;

#define VGAP_IDX          0x03C4
#define VGAP_DATA         0x03C5
#define VGAP_PALETTE_MASK 0x03C6
#define VGAP_REG_READ     0x03C7
#define VGAP_REG_WRITE    0x03C8
#define VGAP_PALETTE_DATA 0x03C9

#define VGAR_MAP_MASK 0x02

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) :"memory");
    /* There's an outb %al, $imm8  encoding, for compile-time constant port numbers that fit in 8b.  (N constraint).
     * Wider immediate constants would be truncated at assemble-time (e.g. "i" constraint).
     * The  outb  %al, %dx  encoding is the only option for all other cases.
     * %1 expands to %dx because  port  is a uint16_t.  %w1 could be used if we had the port number a wider C type */
}

static inline uint8_t inb(uint16_t port) {
	uint8_t ret;
	asm volatile ( "inb %1, %0"
					 : "=a"(ret)
					 : "Nd"(port)
					 : "memory");
	return ret;
}

#ifdef TOSLIKE
float time_diff(TIME_TYPE start, TIME_TYPE end) {
	return (double)(end-start)/g_tos.hpet.hpet_freq;
}

TIME_TYPE get_time() {
	return *g_tos.hpet.hpet_addr-g_tos.hpet.hpet_initial;
}
#endif

static void init_mutex(mu_t* mu) {
	mu->lock = 0;
}

#if 0
void lock_mutex(mu_t* mutex) {
	while (__atomic_test_and_set(&mutex->lock, __ATOMIC_SEQ_CST)) {
		// Spin while the lock is held
		_mm_pause(); // Use the PAUSE instruction for better spin-wait performance
	}
}
#endif

static bool mu_try_lock(mu_t* mutex) {
	/* myprintf("[mu_try_lock]\n"); */
	return __atomic_test_and_set(&mutex->lock, __ATOMIC_SEQ_CST);
}

static void mu_clear(mu_t* mutex) {
	/* myprintf("[mu_clear]\n"); */
	__atomic_clear(&mutex->lock, __ATOMIC_SEQ_CST);
}

extern engine_threads_t* ge;
void my_irq_isr() {
	tos_sound_update(&ge->e);
}

/* TODO maybe move this into engine_t */
static void (*tos_irq_isr)();

extern void isr_wrapper(void);

void tos_sound_init(engine_t* const e) {
#ifdef VERBOSE
	myprintf("[tos_sound_init]\n");
#endif
	tos_irq_isr = TOS_GetIRQ(0x20);
#ifdef VERBOSE
	myprintf("[tos_sound_init] tos_irq_isr:%p isr_wrapper:%p %p\n", tos_irq_isr, isr_wrapper, &isr_wrapper);
#endif

	/* QEMU freaks out if we just hammer on the PC speaker, so we track the state interally */
	/* turn off PC speaker to ensure consistent starting state */
	u32 tmp = inb(0x61);
	outb(0x61, tmp & 0xFC);
	e->tos_snd_spkr = 0;
	e->tos_sample_start_t = get_time();

	TOS_SetIRQ(0x20, isr_wrapper);

	// Set the PIT command byte
	outb(0x43, 0x34); // Channel 0, mode 2 (rate generator), lobyte/hibyte

	// Set the PIT counter value (divisor)
	const u16 divisor = 1193180 / 11025;
	outb(0x40, (uint8_t)(divisor & 0xFF));           // Low byte
	outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));    // High byte

	init_mutex(&e->tos_snd_mu);
	e->tos_snd_pos = 0;
	snd_callback(e, (void*)e->tos_snd_buf, SND_SAMPLES);
}

void tos_sound_update(engine_t* const e) {
#if 0
	if (mu_try_lock(&e->tos_snd_mu))
		return;
#endif
#ifdef VERBOSE
	myprintf("[tos_sound_update]\n");
#endif
	const TIME_TYPE test_t = get_time();
	const double ideal_delay = 1.0/SND_FREQ;
	const double test_dt = time_diff(e->tos_sample_start_t, test_t);
	if (test_dt < ideal_delay) {
		/* mu_clear(&e->tos_snd_mu); */
		return;
	}
#if 0
	if (test_dt-ideal_delay > 0.001) {
		myprintf("slow:%f\n", test_dt);
	}
#endif
	e->tos_sample_start_t = test_t;

	const i16 val = e->tos_snd_buf[e->tos_snd_pos];

	if (val > 0x0) {
		e->tos_snd_spkr = 1;
#define PIT_OSC 1193180
		const u64 div = PIT_OSC / val;
		outb(0x43, 0xb6);
		outb(0x42, div);
		outb(0x42, div>>8);
		u32 tmp = inb(0x61);
		if ((tmp&3) == 0) {
			tmp = tmp|3;
			outb(0x61, tmp);
		}
	} else if (e->tos_snd_spkr) {
		e->tos_snd_spkr = 0;
		// Turn Off
		u32 tmp = inb(0x61);
		outb(0x61, tmp & 0xFC);
	}

	e->tos_snd_pos++;
	if (e->tos_snd_pos >= SND_SAMPLES) {
		e->tos_snd_pos = 0;
		snd_callback(e, (void*)e->tos_snd_buf, SND_SAMPLES);
	}

	/* mu_clear(&e->tos_snd_mu); */
}

static void vga_update_palette(u8 color_num, palette_color_t color) {
	/* Terry disables interrupts while he does this, I don't */
	outb(VGAP_PALETTE_MASK,0xff);
	outb(VGAP_REG_WRITE,color_num);
	/* VGA is 6bit colors, so shift */
	outb(VGAP_PALETTE_DATA,color.rgba.r>>2);
	outb(VGAP_PALETTE_DATA,color.rgba.g>>2);
	outb(VGAP_PALETTE_DATA,color.rgba.b>>2);
}

static inline void vga_update_palettes(const palette_t* const palette) {
	for (u8 i=0; i<16; i++) {
		vga_update_palette(i, palette->colors[i]);
	}
}

static inline void write_vga(const uint8_t* const fb, const int interlace) {
	/* static int flip = 0; */
	static u8 plane_cache[SCREEN_W*SCREEN_H/2];

#if 0
	static u8 fb_cache[SCREEN_W*SCREEN_H];
	static u8 dirty[SCREEN_W*SCREEN_H/8];
	/* Update FB Cache and Mark Dirty */
	/* memset(dirty, 0x00, sizeof(dirty)); */
	for (int y=8; y<SCREEN_H; y++) {
		const u8* pfb = fb + y*SCREEN_W;
		u8* pfb_cache = fb_cache + y*SCREEN_W;
		for (int x=0; x<SCREEN_W/8; x++, pfb+=8, pfb_cache+=8) {
			for (int i=0; i<8; i++) {
				if (pfb_cache[i] != pfb[i]) {
					pfb_cache[i] = pfb[i];
					bitarr_set(dirty, y*SCREEN_W+x);
				}
			}
		}
	}
#endif

	/* Regenerate Plane Cache */
	memset(plane_cache, 0, sizeof(plane_cache));
	for (int plane=0; plane<4; plane++) {
		u8* pplane = &plane_cache[plane*SCREEN_W*SCREEN_H/8];
		for (int y=0; y<SCREEN_H; y++) {
			for (int x=0; x<SCREEN_W/8; x++, pplane++) {
				for (int i=0; i<8; i++) {
					*pplane |= ((fb[y*SCREEN_W+x*8+i]&(1u<<plane)) >>(plane)) <<(7-i);
				}
			}
		}
	}

	/* vsync */
	/* while (inb(0x3da)&8) {} */
	/* while (!(inb(0x3da)&8)) {} */

	/* Transfer Memory to VGA */
	for (int y=0; y<SCREEN_H; y++) {
		/* if ((y+flip*2) % 3) continue; // interlace */
		if ((y+interlace) % 2) continue;
		for (int plane=0; plane<4; plane++) {
			u8* pp = g_tos.vga_mem;
			const u8* const pc = &plane_cache[plane*SCREEN_W*SCREEN_H/8];
			outb(VGAP_IDX, VGAR_MAP_MASK);
			outb(VGAP_DATA, 1u<<plane);
#if 0
			for (int x=0; x<SCREEN_W/8; x++) {
				const size_t offset = y*SCREEN_W+x;
				if (bitarr_get(dirty, offset)) {
					/* bitarr_clear(dirty, offset); */
					const size_t idx = (y*SCREEN_W + x*8)/8;
					/* *(uint64_t*)(&pp[idx]) = *(uint64_t*)(&pc[idx]); */
					pp[idx] = pc[idx];
				}
			}
#else
			memcpy(&pp[y*SCREEN_W/8], &pc[y*SCREEN_W/8], 640/8);
#endif
		}
		/* memset(dirty+y*SCREEN_W/8, 0, 640/8); */
	}

#if 0
	for (int plane=0; plane<4; plane++) {
		const u8* const pc = plane_cache + plane*SCREEN_W*SCREEN_H/8;
		outb(VGAP_IDX, VGAR_MAP_MASK);
		outb(VGAP_DATA, 1<<plane);
		memcpy(vga_mem, pc, 640*480/8);
	}
#endif

	/* flip = !flip; */
}

void tos_get_statusline(char* str) {
#if 0
	size_t mem_free = g_tos.bp_code->alloced_u8s - g_tos.bp_code->used_u8s;
	if (g_tos.bp_data)
		mem_free += g_tos.bp_data->alloced_u8s - g_tos.bp_data->used_u8s;
#endif

	/* Generate Status Line */
	str += stbsp_snprintf(str, 64, "[TempleOS Legit] MUsed:%08lX", g_tos.bp_code->used_u8s+g_tos.bp_data->used_u8s);
	/* str += stbsp_snprintf(str, 64, "MCode:%010lX/%010lX MData:%010lX/%010lX CPU ", g_tos.bp_code->used_u8s, g_tos.bp_code->alloced_u8s, g_tos.bp_data->used_u8s, g_tos.bp_data->alloced_u8s); */
#if 0
	for (i32 i=0; i<cpu_cnt; i++) {
		str += stbsp_sprintf(str, "%.2f ", g_tos.ccpu[i].idle_factor);
	}
#endif
}

int tosfb_thread(void* data) {
	tosfb_thread_t* const d = data;
	u8* const fb = d->fb;
	volatile const i8* const interlace = &d->e->interlace;
#ifndef NDEBUG
	myprintf("[tosfb_thread] start data:%lx quit:%d fb:%lx\n", d, d->quit, d->fb);
#endif
	while (!d->quit) {
		LockMutex(d->mutex);
		while (!d->draw) {
			CondWait(d->mu_cond, d->mutex);
		}
		d->draw = 0;
		UnlockMutex(d->mutex);

		/* Update Palette */
		if (d->update_palette) {
#ifndef NDEBUG
			fputs("[tosfb] updating palette\n", stdout);
#endif
			d->update_palette = 0;
			vga_update_palettes(&d->palette);
		}

		/* Draw Framebuffer */
		write_vga(fb, *interlace);
	}

	/* Quitting, so revert PIT and IRQ changes */
	outb(0x42,0x34);
	outb(0x40,(u8)SYS_TIMER0_PERIOD);
	outb(0x40,SYS_TIMER0_PERIOD>>8);
	TOS_SetIRQ(0x20, tos_irq_isr);

#ifdef VERBOSE
	myprintf("[tosfb_thread] quitting\n");
#endif
	return 0;
}

void tosfb_init(tosfb_thread_t* thr, engine_t* const e, const palette_t* const palette) {
#ifndef NDEBUG
	myprintf("[tosfb_init] thr:%lx\n", thr);
#endif

	thr->e = e;
	thr->palette = *palette;

	get_tos_data(&g_tos);

	/* Clear */
	outb(VGAP_IDX, VGAR_MAP_MASK);
	outb(VGAP_DATA, 0xf);
	u8* pp = g_tos.vga_mem + SCREEN_W;
	for (int i=SCREEN_W; i<SCREEN_W*SCREEN_H/8; i++, pp++) {
		*pp = 0;
	}

	/* Thread */
	thr->draw = 0;
	thr->update_palette = 1;
	thr->quit = 0;
	thr->mutex = CreateMutex();
	thr->mu_cond = CreateCond();
	thr->thread = CreateThread(&tosfb_thread, "fb", thr);
}
