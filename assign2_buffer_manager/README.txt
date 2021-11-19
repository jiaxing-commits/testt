Group 3 - The Best Group
Teammates: Juseung Lee, Kunal Patil, Jiaxing Wu

#This file records the implementation of buffer manager for assignment 2#
---------------------------------------------------------------------------------------------
Description: The purpose of the buffer manager is to manage pages used in the memory. The 
buffer manager manages these pages in structures called buffer pools, which behaves like 
cache in the CPU. 

In this project, there are two main structures and three sets of functions that are dealt with. 
The two main structures are BM_BufferPool (the buffer pool) and BM_PageHandle (the pages that 
are managed). Then, the buffer manager has three sets of functions, which are Buffer Pool 
Functions, Page Management Functions, and Statistics Functions.

Buffer Pool Functions are the functions that initializes, flushes, or destroys a buffer pool.

Page Management Functions are most of the core functions of the buffer manager. These functions
can pin, unpin, mark(dirty), or flush a page. Just like how the cache needs replacement policies, 
buffer managers also have replacement policies for pages in the buffer pool. There are two 
replacement policies that we implemented: FIFO(first in first out) and LRU(least recently used).
FIFO replaces pages based on history/timeline of the page in the buffer page. LRU replaces pages
based on least usage count of the page in buffer page. Both of these replacement policies are 
used in the pinning and unpinning pages.

Finally, there are the Statistics Functions. These functions allow the user to gather general 
information about the status of the buffer pool. They include returning a list of page numbers
of all pages in the buffer pool, returning a list of dirty flags of all pages in the buffer 
pool, returning a list of fix counts of all pages in the buffer pool, returning the number of 
read operations performed with the buffer pool, and returning the number of write operations
performed with the buffer pool.
--------------------------------------------------------------------------------------------
Testing and Running: All of the tests are administered through the test_assign2_1.c file.

To compile and run the tests, type:
make

To clean up after tests, type:
make clean