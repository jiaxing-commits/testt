#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include "storage_mgr.h"
#include "buffer_mgr.h"

int hit = 0, num_reads = 0, num_writes = 0;

typedef struct BM_PageFrame {
	SM_PageHandle data;
	PageNumber pageNum;
	int dirty_bit, fix_count, hit_count, reference_count;
} BM_PageFrame;

void FIFO(BM_BufferPool *const bm, BM_PageFrame *page) {

	SM_FileHandle file_handle;
	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;

	// Boring. first in first out queue. you know the deal
	for (int i = 0, left = num_reads % bm->numPages; i < bm->numPages; i++, left++) {

		if (page_frame[left].fix_count == 0) {

			if (page_frame[left].dirty_bit == 1) {
				// page updated so write it to disk
				openPageFile(bm->pageFile, &file_handle);
				writeBlock(page_frame[left].pageNum, &file_handle, page_frame[left].data);
				num_writes++;
			}

			page_frame[left].data = page->data;
			page_frame[left].pageNum = page->pageNum;
			page_frame[left].dirty_bit = page->dirty_bit;
			page_frame[left].fix_count = page->fix_count;

			break;
		}

		if (left % bm->numPages == 0) left = 0;
	}
}

void LRU(BM_BufferPool *const bm, BM_PageFrame *page) {

	SM_FileHandle file_handle;
	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	int least_hit_idx = -1;
	int least_hit_count = INT_MAX;

	// Find the least hit count and the index of that page
	for (int i = 0; i < bm->numPages; i++) {

		if (page_frame[i].hit_count < least_hit_count) {
			least_hit_count = page_frame[i].hit_count;
			least_hit_idx = i;
		}
	}

	// if dirty, we flush
	if (page_frame[least_hit_idx].dirty_bit == 1) {
		openPageFile(bm->pageFile, &file_handle);
		writeBlock(page_frame[least_hit_idx].pageNum, &file_handle, page_frame[least_hit_idx].data);
		num_writes++;
	}

	// return page info
	page_frame[least_hit_idx].data = page->data;
	page_frame[least_hit_idx].pageNum = page->pageNum;
	page_frame[least_hit_idx].dirty_bit = page->dirty_bit;
	page_frame[least_hit_idx].fix_count = page->fix_count;
	page_frame[least_hit_idx].hit_count = page->hit_count;
}

// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData){	

	// initializing the buffer pool and global variables
	BM_PageFrame *page_frames = malloc(sizeof(BM_PageFrame) * numPages);
	bm->pageFile = (char *) pageFileName, bm->strategy = strategy, bm->numPages = numPages;

	for (int i = 0; i < bm->numPages; i++) {
		page_frames[i].data = NULL;
		page_frames[i].pageNum = -1;
		page_frames[i].dirty_bit = 0, page_frames[i].fix_count = 0, page_frames[i].hit_count = 0, page_frames[i].reference_count = 0;
	}

	bm->mgmtData = page_frames;

	num_reads = 0, num_writes = 0;

	return RC_OK;
}

// flushing buffer pool and freeing variables
RC shutdownBufferPool(BM_BufferPool *const bm) {

	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;

	forceFlushPool(bm); // cannot shutdown if some pages remain pinned

	for (int i = 0; i < bm->numPages; i++) if (page_frame[i].fix_count != 0) return RC_PAGES_STILL_PINNED;

	// free allocated mem
	free(page_frame);
	bm->mgmtData = NULL;

	return RC_OK;
}

// loop through buffer pool and flush all unpinned pages
RC forceFlushPool(BM_BufferPool *const bm) {

	SM_FileHandle file_handle;
	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;

	for (int i = 0; i < bm->numPages; i++) {

		if (page_frame[i].fix_count == 0 && page_frame[i].dirty_bit == 1) {

			openPageFile(bm->pageFile, &file_handle);
			writeBlock(page_frame[i].pageNum, &file_handle, page_frame[i].data);

			page_frame[i].dirty_bit = 0; // set the dirty bit 0 in order to indicate it is not dirty
			num_writes = num_writes + 1;
		}
	}
		
	return RC_OK;
}

// Buffer Manager Interface Access Pages

// find page in buffer pool and make dirty
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page) {

	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	PageNumber pNum = page->pageNum;
	RC result = RC_MARK_DIRTY_ERROR;

	for (int i = 0; i < bm->numPages; i++) {

		if (page_frame[i].pageNum == pNum) { // find page in buffer pool
			page_frame[i].dirty_bit = 1; // set the dirty bit 1 in order to indicate it is dirty
			result = RC_OK;
		}
	}		

	return result;
}
// find page in buffer pool and unpin
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page) {

	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	PageNumber pNum = page->pageNum;
	RC result = RC_UNPIN_PAGE_ERROR;

	for (int i = 0; i < bm->numPages; i++) {

		if(page_frame[i].pageNum == pNum) {
			page_frame[i].fix_count = page_frame[i].fix_count - 1;
			result = RC_OK;
		}
	}

	return result;
}

// find page in buffer pool and force flush page
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page) {

	SM_FileHandle file_handle;
	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	PageNumber pNum = page->pageNum;

	for (int i = 0; i < bm->numPages; i++) {

		if (page_frame[i].pageNum == pNum) {
			openPageFile(bm->pageFile, &file_handle);
			writeBlock(page_frame[i].pageNum, &file_handle, page_frame[i].data); // write the current content of the page back to the page file on disk

			page_frame[i].dirty_bit = 0; // set the dirty bit 0 in order to indicate it is not dirty
			num_writes = num_writes + 1; // increment write count
		}
	}

	return RC_OK;
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum){

	SM_FileHandle file_handle;
	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	bool full = true;

	if (page_frame[0].pageNum == -1) { // if we couldn't found page

		// first page to be pinned
		openPageFile(bm->pageFile, &file_handle);
		page_frame[0].data = (SM_PageHandle) malloc(PAGE_SIZE);
		ensureCapacity(pageNum, &file_handle);
		readBlock(pageNum, &file_handle, page_frame[0].data);

		// update page properties
		page_frame[0].pageNum = pageNum;
		page_frame[0].fix_count++;

		num_reads = 0, hit = 0;

		page_frame[0].hit_count = hit;
		page_frame[0].reference_count = 0;

		page->pageNum = pageNum;
		page->data = page_frame[0].data;

		return RC_OK; // pin page successful
	}

	// if we found page and buffer is not full
	for (int i = 0; i < bm->numPages; i++) {

		if (page_frame[i].pageNum != -1) {

			if (page_frame[i].pageNum == pageNum) {

				page_frame[i].fix_count++, hit++;
				full = false;

				// replacement strategy-specfic cases
				if (bm->strategy == RS_LRU) page_frame[i].hit_count = hit;

				page->pageNum = pageNum;
				page->data = page_frame[i].data;

				break;
			}

		} else {

			openPageFile(bm->pageFile, &file_handle);
			page_frame[i].data = (SM_PageHandle) malloc(PAGE_SIZE);
			readBlock(pageNum, &file_handle, page_frame[i].data);

			page_frame[i].pageNum = pageNum;
			page_frame[i].fix_count = 1;
			page_frame[i].reference_count = 0;

			num_reads++, hit++;

			if (bm->strategy == RS_LRU) page_frame[i].hit_count = hit;

			page->pageNum = pageNum, page->data = page_frame[i].data;

			full = false;

			break;
		}
	}

	if(full) { // a page which already exists in buffer pool is to be replaced since pool is full

		BM_PageFrame *new_page = (BM_PageFrame *) malloc(sizeof(BM_PageFrame));

		openPageFile(bm->pageFile, &file_handle); // open new page
		new_page->data = (SM_PageHandle) malloc(PAGE_SIZE); // allocate memory
		readBlock(pageNum, &file_handle, new_page->data);

		new_page->pageNum = pageNum, new_page->dirty_bit = 0, new_page->reference_count = 0, new_page->fix_count = 1;
		num_reads++, hit++;

		if (bm->strategy == RS_LRU) new_page->hit_count = hit;

		page->pageNum = pageNum, page->data = new_page->data;

		ReplacementStrategy bmStrategy = bm->strategy;
		if(bmStrategy == 0) FIFO(bm, new_page); // FIFO Strategy
		else if(bmStrategy == 1) LRU(bm, new_page); // LRU Strategy
		else return RC_NO_STRATEGY_FOUND;
	}

	return RC_OK;
}

// Statistics Interface

// go through buffer pool and return all page numbers if they exist
PageNumber *getFrameContents (BM_BufferPool *const bm) {

	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	PageNumber *page_numbers = malloc (sizeof(PageNumber) * bm->numPages);

	for (int i = 0; i <  bm->numPages; i++) {
		if (page_frame[i].pageNum == -1) page_numbers[i] = NO_PAGE; // if the page does not exist
		else page_numbers[i] = page_frame[i].pageNum;
	}
	
	return page_numbers;
}

// go through buffer pool and return all dirty bits
bool *getDirtyFlags (BM_BufferPool *const bm) {

	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	bool *dirty_flags = malloc(sizeof(bool) * bm->numPages);

	for (int i = 0; i < bm->numPages; i++) {
		if (page_frame[i].dirty_bit == 1) dirty_flags[i] = true; // if current page is dirty
		else dirty_flags[i] = false; // if current page is empty page frame
	}

	return dirty_flags;
}

// go through buffer pool and return all fix counts
int *getFixCounts (BM_BufferPool *const bm) {

	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	PageNumber *fix_counts = malloc (sizeof(PageNumber) * bm->numPages);

	for (int i = 0; i < bm->numPages; i++) {
		if (page_frame[i].fix_count != -1) fix_counts[i] = page_frame[i].fix_count;
		else fix_counts[i] = 0; // 0 for empty page frames
	}

	return fix_counts;
}

// return number of reads
int getNumReadIO (BM_BufferPool *const bm) { return num_reads + 1; }

// return number of writes
int getNumWriteIO (BM_BufferPool *const bm) { return num_writes; }