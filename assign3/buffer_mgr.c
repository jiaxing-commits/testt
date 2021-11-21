#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "storage_mgr.h"
#include "buffer_mgr.h"

typedef struct BM_PageFrame {
	PageNumber pageNum;
	SM_PageHandle data;
	int fix_count, dirty_bit, hit_count, reference_count;
	int global_hit, num_reads, num_writes;
} BM_PageFrame;

// FIFO Strategy-replace page that has been in the buffer for the longest time
void FIFO(BM_BufferPool *const bm, BM_PageFrame *page) {

	SM_FileHandle file_handle;
	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;

	// Boring. first in first out queue. you know the deal
	for (int i = 0, left = page_frame->num_reads % bm->numPages; i < bm->numPages; i++, left++) {

		if (page_frame[left].fix_count == 0) {

			if (page_frame[left].dirty_bit == 1) {
				// page updated so write it to disk
				openPageFile(bm->pageFile, &file_handle);
				writeBlock(page_frame[left].pageNum, &file_handle, page_frame[left].data);
				page_frame->num_writes++;
			}

			page_frame[left].fix_count = page->fix_count;
			page_frame[left].dirty_bit = page->dirty_bit;
			page_frame[left].pageNum = page->pageNum;
			page_frame[left].data = page->data;

			break;
		}

		if (left % bm->numPages == 0) left = 0;
	}
}

// LRU Strategy-the page that has not been used for the longes time in memory will be selected for replacement
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
		page_frame->num_writes++;
		openPageFile(bm->pageFile, &file_handle);
		writeBlock(page_frame[least_hit_idx].pageNum, &file_handle, page_frame[least_hit_idx].data);
	}

	// return page info
	page_frame[least_hit_idx].fix_count = page->fix_count;
	page_frame[least_hit_idx].dirty_bit = page->dirty_bit;
	page_frame[least_hit_idx].hit_count = page->hit_count;
	page_frame[least_hit_idx].pageNum = page->pageNum;
	page_frame[least_hit_idx].data = page->data;
}

// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData){	

	// initializing the buffer pool and global variables
	BM_PageFrame *page_frames = malloc(sizeof(BM_PageFrame) * numPages);
	bm->numPages = numPages, bm->strategy = strategy, bm->pageFile = (char *) pageFileName;

	for (int i = 0; i < bm->numPages; i++) {
		page_frames[i].data = NULL;
		page_frames[i].pageNum = -1;
		page_frames[i].fix_count = 0, page_frames[i].dirty_bit = 0, page_frames[i].hit_count = 0, page_frames[i].reference_count = 0;
	}

	page_frames->num_reads = 0, page_frames->num_writes = 0;
	bm->mgmtData = page_frames;

	return RC_OK;
}

// flushing buffer pool and freeing variables
RC shutdownBufferPool(BM_BufferPool *const bm) {
	RC check = RC_OK;

	check = forceFlushPool(bm); // cannot shutdown if some pages remain pinned
	if (check != RC_OK) return RC_FORCE_FLUSH_FAILED;
	
	// return the error message if the page is still pinned
	for (int i = 0; i < bm->numPages; i++) if (((BM_PageFrame *) bm->mgmtData)[i].fix_count != 0) return RC_PAGES_STILL_PINNED;

	// free allocated mem
	free(((BM_PageFrame *) bm->mgmtData));
	return check;
}

// loop through buffer pool and flush all unpinned pages
RC forceFlushPool(BM_BufferPool *const bm) {
	SM_FileHandle file_handle;

	for (int i = 0; i < bm->numPages; i++) {

		if (((BM_PageFrame *) bm->mgmtData)[i].fix_count == 0 ){ // check if the page is unpinned
			if (((BM_PageFrame *) bm->mgmtData)[i].dirty_bit == 1) { // if the page is dirty
				((BM_PageFrame *) bm->mgmtData)->num_writes += 1; // increment write count
				((BM_PageFrame *) bm->mgmtData)[i].dirty_bit = 0; // set the dirty bit 0 in order to indicate it is not dirty
				
				openPageFile(bm->pageFile, &file_handle);
				writeBlock(((BM_PageFrame *) bm->mgmtData)[i].pageNum, &file_handle, ((BM_PageFrame *) bm->mgmtData)[i].data);

			}
		}
	}
		
	return RC_OK;
}

// Buffer Manager Interface Access Pages

// find page in buffer pool and make dirty
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page) {
	PageNumber pNum = page->pageNum; // get the page number
	RC result = RC_MARK_DIRTY_ERROR;

	for (int i = 0; i < bm->numPages; i++) {

		if (((BM_PageFrame *) bm->mgmtData)[i].pageNum == pNum) { // find page in buffer pool
			((BM_PageFrame *) bm->mgmtData)[i].dirty_bit = 1; // set the dirty bit 1 in order to indicate it is dirty
			result = RC_OK;
		}
	}		

	return result;
}

// find page in buffer pool and unpin
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page) {
	PageNumber pNum = page->pageNum; // get the page number
	RC result = RC_UNPIN_PAGE_ERROR;

	for (int i = 0; i < bm->numPages; i++) {

		if(((BM_PageFrame *) bm->mgmtData)[i].pageNum == pNum) { // find page in buffer pool
			((BM_PageFrame *) bm->mgmtData)[i].fix_count = ((BM_PageFrame *) bm->mgmtData)[i].fix_count - 1; // unpin the page by decrementing fix count
			result = RC_OK;
		}
	}

	return result;
}

// find page in buffer pool and force flush page
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page) {

	SM_FileHandle file_handle;
	PageNumber pNum = page->pageNum; // get the page number

	for (int i = 0; i < bm->numPages; i++) {

		if (((BM_PageFrame *) bm->mgmtData)[i].pageNum == pNum) { // find page in buffer pool
			((BM_PageFrame *) bm->mgmtData)[i].dirty_bit = 0; // set the dirty bit 0 in order to indicate it is not dirty
			((BM_PageFrame *) bm->mgmtData)->num_writes += 1; // increment write count

			openPageFile(bm->pageFile, &file_handle);
			writeBlock(((BM_PageFrame *) bm->mgmtData)[i].pageNum, &file_handle, ((BM_PageFrame *) bm->mgmtData)[i].data); // write the current content of the page back to the page file on disk
		}
	}

	return RC_OK;
}

RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum) {

	BM_PageFrame *page_frame = (BM_PageFrame *) bm->mgmtData;
	SM_FileHandle file_handle;
	bool not_full = false;

	if (page_frame[0].pageNum != -1) {
		// if we found page and buffer is not full
		for (int i = 0; i < bm->numPages; i++) {
			
			if (page_frame[i].pageNum >= 0) {

				if (page_frame[i].pageNum == pageNum) {

					page_frame->global_hit++, page_frame[i].fix_count++;

					if (bm->strategy == RS_LRU) page_frame[i].hit_count = page_frame->global_hit;

					page->data = page_frame[i].data, page->pageNum = pageNum;

					not_full = true;

					break;
				}
				
			} else {

				page_frame[i].reference_count = 0, page_frame[i].fix_count = 1;
				page_frame[i].pageNum = pageNum;

				openPageFile(bm->pageFile, &file_handle);

				page_frame[i].data = (SM_PageHandle) malloc(PAGE_SIZE);

				readBlock(pageNum, &file_handle, page_frame[i].data);

				page_frame->global_hit++, page_frame->num_reads++;
				// replacement strategy-specfic case for LRU
				if (bm->strategy == RS_LRU) page_frame[i].hit_count = page_frame->global_hit;

				page->pageNum = pageNum, page->data = page_frame[i].data;

				not_full = true;

				break;
			}
		}

		if (!not_full) { // a page which already exists in buffer pool is to be replaced since pool is full

			BM_PageFrame *new_page = (BM_PageFrame *) malloc(sizeof(BM_PageFrame));
			openPageFile(bm->pageFile, &file_handle); // open new page
			new_page->data = (SM_PageHandle) malloc(PAGE_SIZE); // allocate memory
			readBlock(pageNum, &file_handle, new_page->data);

			new_page->reference_count = 0, new_page->dirty_bit = 0, new_page->fix_count = 1;
			page_frame->global_hit++, page_frame->num_reads++;
			new_page->pageNum = pageNum, page->pageNum = pageNum;
			page->data = new_page->data;

			if (bm->strategy == RS_FIFO) FIFO(bm, new_page);
			else if (bm->strategy == RS_LRU) {
				new_page->hit_count = page_frame->global_hit;
				LRU(bm, new_page);
			}
		}
		
	} else {
		openPageFile(bm->pageFile, &file_handle);
		// first page to be pinned
		page_frame[0].data = (SM_PageHandle) malloc(PAGE_SIZE);
		readBlock(pageNum, &file_handle, page_frame[0].data);

		// update page properties
		page->pageNum = pageNum, page_frame[0].pageNum = pageNum;

		page_frame->global_hit = 0, page_frame->num_reads = 0, page_frame[0].reference_count = 0;

		page_frame[0].hit_count = page_frame->global_hit;

		page->data = page_frame[0].data;
		page_frame[0].fix_count++;


	}
	return RC_OK;
}

// Statistics Interface

// go through buffer pool and return all page numbers if they exist
PageNumber *getFrameContents (BM_BufferPool *const bm) {
	PageNumber *page_numbers = malloc (sizeof(PageNumber) * bm->numPages);

	for (int i = 0; i <  bm->numPages; i++) {
		if (((BM_PageFrame *) bm->mgmtData)[i].pageNum == -1) page_numbers[i] = NO_PAGE; // if the page does not exist
		else page_numbers[i] = ((BM_PageFrame *) bm->mgmtData)[i].pageNum;
	}
	
	return page_numbers;
}

// go through buffer pool and return all dirty bits
bool *getDirtyFlags (BM_BufferPool *const bm) {
	bool *dirty_flags = malloc(bm->numPages * sizeof(bool));

	for (int i = 0; i < bm->numPages; i++) {
		if (((BM_PageFrame *) bm->mgmtData)[i].dirty_bit == 1) dirty_flags[i] = true; // if current page is dirty
		else dirty_flags[i] = false; // if current page is empty page frame
	}

	return dirty_flags;
}

// go through buffer pool and return all fix counts
int *getFixCounts (BM_BufferPool *const bm) {
	PageNumber *fix_counts = malloc ( bm->numPages * sizeof(PageNumber));

	for (int i = 0; i < bm->numPages; i++) {
		if (((BM_PageFrame *) bm->mgmtData)[i].fix_count != -1) fix_counts[i] = ((BM_PageFrame *) bm->mgmtData)[i].fix_count;
		else fix_counts[i] = 0; // 0 for empty page frames
	}
	
	return fix_counts;
}

// return number of reads
int getNumReadIO (BM_BufferPool *const bm) { return ((BM_PageFrame *) bm->mgmtData)->num_reads + 1; }

// return number of writes
int getNumWriteIO (BM_BufferPool *const bm) { return ((BM_PageFrame *) bm->mgmtData)->num_writes; }