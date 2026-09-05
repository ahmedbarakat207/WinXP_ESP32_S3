/* SD-backed guest RAM pager (see swap.h). Plain C99 + stdio only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "swap.h"

#define PAGE_SHIFT 12
#define PAGE_SIZE 4096

#define F_DIRTY    1  /* host-side modified, must be written back */
#define F_HAS_COPY 2  /* valid copy exists in the backing file */
#define F_PINNED   4  /* never evict */

struct Swap {
	uint32_t npages;
	uint32_t nframes;
	uint32_t npinned;
	FILE *f;
	void *cb_opaque;
	void (*invalidate)(void *, uint32_t);
	uint8_t *frames;        /* nframes * 4096, frame i at frames + i*4096 */
	int32_t *pg_frame;      /* guest page -> frame idx, -1 = swapped out */
	int32_t *fr_page;       /* frame idx -> guest page */
	uint8_t *pg_flags;
	uint8_t *pg_use;        /* clock reference bits */
	uint32_t clock;         /* clock hand (page number) */
	uint8_t *zero_page;     /* reads past the end return zeros */
	uint8_t *scratch_page;  /* writes past the end are discarded */
};

Swap *swap_create(uint32_t total_bytes, uint32_t resident_bytes,
		  const char *path, uint32_t pin_bytes,
		  void *cb_opaque, void (*invalidate)(void *, uint32_t),
		  void *(*alloc)(long size))
{
	assert(total_bytes % PAGE_SIZE == 0);
	assert(resident_bytes % PAGE_SIZE == 0);
	assert(pin_bytes % PAGE_SIZE == 0);
	assert(resident_bytes >= pin_bytes && pin_bytes > 0);
	assert(total_bytes >= resident_bytes);

	Swap *s = alloc(sizeof(*s));
	memset(s, 0, sizeof(*s));
	s->npages = total_bytes / PAGE_SIZE;
	s->nframes = resident_bytes / PAGE_SIZE;
	s->npinned = pin_bytes / PAGE_SIZE;
	s->cb_opaque = cb_opaque;
	s->invalidate = invalidate;

	s->frames = alloc((long) s->nframes * PAGE_SIZE);
	memset(s->frames, 0, (size_t) s->nframes * PAGE_SIZE);
	s->pg_frame = alloc((long) s->npages * sizeof(int32_t));
	s->fr_page = alloc((long) s->nframes * sizeof(int32_t));
	s->pg_flags = alloc(s->npages);
	s->pg_use = alloc(s->npages);
	memset(s->pg_flags, 0, s->npages);
	memset(s->pg_use, 0, s->npages);
	s->zero_page = alloc(PAGE_SIZE);
	memset(s->zero_page, 0, PAGE_SIZE);
	s->scratch_page = alloc(PAGE_SIZE);

	for (uint32_t i = 0; i < s->npages; i++)
		s->pg_frame[i] = -1;
	for (uint32_t i = 0; i < s->nframes; i++)
		s->fr_page[i] = -1;

	/* Permanently resident prefix, identity mapped to the first frames. */
	for (uint32_t i = 0; i < s->npinned; i++) {
		s->pg_frame[i] = (int32_t) i;
		s->fr_page[i] = (int32_t) i;
		s->pg_flags[i] = F_PINNED;
	}
	s->clock = s->npinned;

	s->f = fopen(path, "w+b");
	if (!s->f) {
		fprintf(stderr, "swap: cannot create %s\n", path);
		abort();
	}
	return s;
}

void swap_destroy(Swap *s)
{
	if (!s)
		return;
	if (s->f)
		fclose(s->f);
	/* frames/metadata come from a bump allocator; nothing to free. */
}

void swap_bind(Swap *s, void *cb_opaque, void (*invalidate)(void *, uint32_t))
{
	s->cb_opaque = cb_opaque;
	s->invalidate = invalidate;
}

static uint64_t slot_off(uint32_t lpgno)
{
	return (uint64_t) lpgno * PAGE_SIZE;
}

/* Evict one page, return a free frame. */
static int32_t swap_evict(Swap *s)
{
	for (;;) {
		/* First pass: free floating frame without evicting anything. */
		for (uint32_t i = s->npinned; i < s->nframes; i++) {
			if (s->fr_page[i] < 0)
				return (int32_t) i;
		}
		/* Clock (second chance). Each pass clears use bits until an
		 * unreferenced page turns up. */
		uint32_t start = s->clock;
		int cleared = 0;
		do {
			uint32_t p = s->clock;
			s->clock++;
			if (s->clock >= s->npages)
				s->clock = s->npinned;
			if (p < s->npinned)
				continue;
			int32_t f = s->pg_frame[p];
			if (f < 0)
				continue; /* swapped out */
			if (s->pg_flags[p] & F_PINNED)
				continue;
			if (s->pg_use[p]) {
				s->pg_use[p] = 0;
				cleared = 1;
				continue;
			}
			/* Victim found. */
			if (s->pg_flags[p] & F_DIRTY) {
				if (fseek(s->f, (long) slot_off(p), SEEK_SET) != 0) {
					fprintf(stderr, "swap: seek failed\n");
					abort();
				}
				if (fwrite(s->frames + (uint32_t) f * PAGE_SIZE,
					   1, PAGE_SIZE, s->f) != PAGE_SIZE) {
					fprintf(stderr, "swap: write failed\n");
					abort();
				}
				s->pg_flags[p] = (s->pg_flags[p] & ~F_DIRTY) | F_HAS_COPY;
			}
			/* Clean without a copy: all zeros, just drop it. */
			s->pg_frame[p] = -1;
			s->fr_page[f] = -1;
			s->invalidate(s->cb_opaque, p);
			return f;
		} while (s->clock != start);
		if (!cleared) {
			fprintf(stderr, "swap: no evictable page\n");
			abort();
		}
	}
}

static void swap_in(Swap *s, uint32_t lpgno, int32_t f)
{
	if (s->pg_flags[lpgno] & F_HAS_COPY) {
		if (fseek(s->f, (long) slot_off(lpgno), SEEK_SET) != 0) {
			fprintf(stderr, "swap: seek failed\n");
			abort();
		}
		if (fread(s->frames + (uint32_t) f * PAGE_SIZE,
			  1, PAGE_SIZE, s->f) != PAGE_SIZE) {
			fprintf(stderr, "swap: read failed\n");
			abort();
		}
	} else {
		memset(s->frames + (uint32_t) f * PAGE_SIZE, 0, PAGE_SIZE);
	}
	s->pg_frame[lpgno] = f;
	s->fr_page[f] = (int32_t) lpgno;
	s->pg_use[lpgno] = 1;
}

uint8_t *swap_ptr(Swap *s, uint32_t paddr, int is_write)
{
	uint32_t lpgno = paddr >> PAGE_SHIFT;
	if (lpgno >= s->npages)
		return (is_write ? s->scratch_page : s->zero_page) +
			(paddr & (PAGE_SIZE - 1));
	int32_t f = s->pg_frame[lpgno];
	if (f < 0) {
		f = swap_evict(s);
		swap_in(s, lpgno, f);
	} else {
		s->pg_use[lpgno] = 1;
	}
	if (is_write)
		s->pg_flags[lpgno] |= F_DIRTY;
	return s->frames + (uint32_t) f * PAGE_SIZE + (paddr & (PAGE_SIZE - 1));
}

void swap_mark_dirty(Swap *s, uint32_t paddr)
{
	uint32_t lpgno = paddr >> PAGE_SHIFT;
	if (lpgno < s->npages && s->pg_frame[lpgno] >= 0)
		s->pg_flags[lpgno] |= F_DIRTY;
}

void swap_touch_page(Swap *s, uint32_t lpgno)
{
	if (lpgno < s->npages && s->pg_frame[lpgno] >= 0)
		s->pg_use[lpgno] = 1;
}

void swap_pin(Swap *s, uint32_t paddr)
{
	uint32_t lpgno = paddr >> PAGE_SHIFT;
	if (lpgno >= s->npages)
		return;
	if (s->pg_frame[lpgno] < 0) {
		int32_t f = swap_evict(s);
		swap_in(s, lpgno, f);
	}
	s->pg_flags[lpgno] |= F_PINNED;
}

void swap_unpin(Swap *s, uint32_t paddr)
{
	uint32_t lpgno = paddr >> PAGE_SHIFT;
	if (lpgno < s->npinned || lpgno >= s->npages)
		return; /* the low prefix stays pinned forever */
	s->pg_flags[lpgno] &= (uint8_t) ~F_PINNED;
}

uint8_t *swap_resident_base(Swap *s)
{
	return s->frames;
}
