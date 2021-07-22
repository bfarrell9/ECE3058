#include "paging.h"
#include "swapops.h"
#include "stats.h"

/*  --------------------------------- PROBLEM 6 --------------------------------------
    Page fault handler.

    When the CPU encounters an invalid address mapping in a page table,
    it invokes the OS via this handler.

    Your job is to put a mapping in place so that the translation can
    succeed. You can use free_frame() to make an available frame.
    Update the page table with the new frame, and don't forget
    to fill in the frame table.

    Lastly, you must fill your newly-mapped page with data. If the page
    has never mapped before, just zero the memory out. Otherwise, the
    data will have been swapped to the disk when the page was
    evicted. Call swap_read() to pull the data back in.

    HINTS:
         - You will need to use the global variable current_process when
           setting the frame table entry.

    ----------------------------------------------------------------------------------
 */
void page_fault(vaddr_t address) {
    /* First, split the faulting address and locate the page table entry */
   vpn_t vpn = vaddr_vpn(address);                          // find vpn value
   pte_t* entry = (pte_t*) (mem + PTBR * PAGE_SIZE);        // create pointer to page entry at virtual addr
   entry += vpn;                                            // add on virtual page number

    /* It's a page fault, so the entry obviously won't be valid. Grab
       a frame to use by calling free_frame(). */
   pfn_t frame = free_frame();                              // find new frame to hold values

    /* Update the page table entry. Make sure you set any relevant bits. */
   entry->dirty = 0;                                        // update dirty (0 since no write)
   entry->valid = 1;                                        // update valid (1 since we are allocating this space)
   entry->pfn = frame;                                      // assign new free frame to pfn

    /* Update the frame table. Make sure you set any relevant bits. */
   frame_table[frame].vpn = vpn;                            // update frame vpn value
   frame_table[frame].mapped = 1;                           // update frame mapped value
   frame_table[frame].process = current_process;            // update frame process value

    /* Initialize the page's memory. On a page fault, it is not enough
     * just to allocate a new frame. We must load in the old data from
     * disk into the frame. If there was no old data on disk, then
     * we need to clear out the memory (why?).
     *
     * 1) Get a pointer to the new frame in memory.
     * 2) If the page has swap set, then we need to load in data from memory
     *    using swap_read().
     * 3) Else, just clear the memory.
     *
     * Otherwise, zero the page's memory. If the page is later written
     * back, swap_write() will automatically allocate a swap entry.
     */
   void* new_frame = (void*) (mem + frame * PAGE_SIZE);     // calculate where new frame will be placed
   if (swap_exists(entry)) swap_read(entry, new_frame);     // if a swap value exists, place entry values into new_frame
   else memset(new_frame, 0, PAGE_SIZE);                    // else, allocate new mem values to 0
   stats.page_faults++;                                     // update segfault statistic

}
