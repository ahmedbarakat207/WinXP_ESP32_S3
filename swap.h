/* SD-backed guest RAM pager: keeps a resident window of 4KB pages in host
 * memory, swaps the rest to a backing file. The low pin_bytes stay resident
 * (firmware/boot code). Untouched pages read back as zeros, so the backing
 * file needs no preallocation. Out-of-range reads return zeros, writes are
 * dropped (same as flat RAM without swap).
 */
#ifndef SWAP_H
#define SWAP_H

#include <stdint.h>

typedef struct Swap Swap;

/* Create a pager. invalidate(opaque, lpgno) drops cached host translations
 * for a guest page (called before reusing an evicted frame). alloc is the
 * frame/metadata allocator. */
Swap *swap_create(uint32_t total_bytes, uint32_t resident_bytes,
		  const char *path, uint32_t pin_bytes,
		  void *cb_opaque, void (*invalidate)(void *, uint32_t),
		  void *(*alloc)(long size));
/* Set (or replace) the invalidate callback after creation (the CPU object
 * often does not exist yet when the frame pool is allocated). */
void swap_bind(Swap *s, void *cb_opaque, void (*invalidate)(void *, uint32_t));
void swap_destroy(Swap *s);

/* Ensure the guest page containing `paddr` is resident and return a host
 * pointer to the byte. The pointer stays valid until the next swap
 * operation on this context (single-threaded use). `is_write` marks the
 * page host-dirty. */
uint8_t *swap_ptr(Swap *s, uint32_t paddr, int is_write);

/* Mark the page containing `paddr` host-dirty (for guest page-table
 * accessed/dirty bit updates done through raw frame pointers). */
void swap_mark_dirty(Swap *s, uint32_t paddr);

/* Record an access to guest page `lpgno` (for clock replacement). */
void swap_touch_page(Swap *s, uint32_t lpgno);

/* Pin/unpin the page containing `paddr` (pinned pages are never evicted). */
void swap_pin(Swap *s, uint32_t paddr);
void swap_unpin(Swap *s, uint32_t paddr);

/* Base of the permanently resident prefix (== frame pool start), for
 * direct access to pinned low memory. */
uint8_t *swap_resident_base(Swap *s);

#endif /* SWAP_H */
