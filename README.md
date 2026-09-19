flwb stands for free list with blocks.

its basically flat container lives in single struct with no outside allocations.

point of flwb is extreme performance with huge number of threads. (>=128)

this performance comes with some trade-offs.

like using way more memory than requested data amount.

like RW on pointers are always valid even after you produce (delete) them. which can cause use-after-recycle.
