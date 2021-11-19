#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "storage_mgr.h"
#include "dberror.h"

/* Global Variables */

FILE *file; // file pointer used across methods

void initStorageManager(void) { return; }

/* Key Functions for manipulating page files */

// createPageFile creates a new page file fileName.
// Initial file size is one page.
// Fills single page with '\0' bytes.
RC createPageFile (char *fileName) {

    RC check = RC_OK;

    // open file for writing
    file = fopen(fileName, "w+");

    // initialize mem as a block of memory of size one PAGE_SIZE
    char *mem = malloc(PAGE_SIZE * sizeof(char));

    if (file == 0) return RC_FILE_NOT_FOUND;

    else {

        // mark allocated memory
        memset(mem, '\0', PAGE_SIZE);

        // wrote allocated memory to file
        fwrite(mem, sizeof(char), PAGE_SIZE, file);

        // free used memory
        free(mem);

        // close file
        check = fclose(file);

        // file created successfully
        return check;
	}
}

// openPageFile Opens an existing page file.
// Returns RC_FILE_NOT_FOUND if the file does not exist.
// If opening file is successful, the fHandle is initialized.
RC openPageFile (char *fileName, SM_FileHandle *fHandle){
    struct stat fileInfo;
    int totalPages;
    RC check = RC_OK;

    file = fopen(fileName, "r+");
    
    if (file == NULL) return RC_FILE_NOT_FOUND;

    // calculate numpages
    if (fstat(fileno(file), &fileInfo) < 0) return RC_OPEN_FILE_ERROR;
    totalPages = fileInfo.st_size / PAGE_SIZE;
    
    // update totalNumPages, filename, and current position
    fHandle->totalNumPages = totalPages;
    fHandle->fileName = fileName;
    fHandle->curPagePos = 0;
    fHandle->mgmtInfo = NULL;
    
    check = fclose(file); // close file
    
    return check; // file opened successfully
}

// closePageFile Closes an open page file
RC closePageFile (SM_FileHandle *fHandle){
    if (file == NULL) return RC_FILE_NOT_FOUND;
    else return RC_OK;
}

// destroyPageFile Destroys/deletes an open page file
RC destroyPageFile (char *fileName) {
    if (remove(fileName) == 0) return RC_OK;
    else return RC_FILE_NOT_FOUND;
}

/* Read Functions for reading blocks from disc*/

// Reads the block at position pageNum from a file
// Stores blocks content in the memory pointed to by memPage
// If file has less than pageNum pages, the method returns RC_READ_NON_EXISTING_PAGE
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){

    RC check = RC_OK;

    if (fHandle == NULL) return RC_FILE_HANDLE_NOT_INIT;

    // page exceeded max page
    if (fHandle->totalNumPages < pageNum || pageNum < 0) return RC_READ_NON_EXISTING_PAGE;
    
    file = fopen(fHandle->fileName, "r+");
    if (file == NULL) return RC_FILE_NOT_FOUND;

    // point file pointer to beginning of file
    if (fseek(file, pageNum * PAGE_SIZE, SEEK_SET) != 0) return RC_SEEK_FAILED;

    // read in memory block
    if (fread(memPage, sizeof(char), PAGE_SIZE, file) < PAGE_SIZE) return RC_READ_FAILED;

    // update page position
    fHandle->curPagePos = ftell(file);

    // close file
    check = fclose(file);

    // file read successful
    return check;
}

// returns the current page position in a file
int getBlockPos (SM_FileHandle *fHandle) { return fHandle->curPagePos; }

// reads the first page in a file
RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) { return readBlock(0, fHandle, memPage); }

// reads the previous page relative to the curPagePos of the file
// curPagePos is moved to the previous page
// returns RC_READ_NON_EXISTING_PAGE if block is before first or after last page
RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) { return readBlock(getBlockPos(fHandle)-1, fHandle, memPage); }

// reads the current page relative to the curPagePos of the file
// curPagePos is moved to the current page
// returns RC_READ_NON_EXISTING_PAGE if block is before first or after last page
RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) { return readBlock(fHandle->curPagePos / PAGE_SIZE,fHandle,memPage); }

// reads the next page relative to the curPagePos of the file
// curPagePos is moved to the next page
// returns RC_READ_NON_EXISTING_PAGE if block is before first or 
RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) { return readBlock(getBlockPos(fHandle)+1, fHandle, memPage); }

// reads the last page in a file
RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) { return readBlock(fHandle->totalNumPages-1, fHandle, memPage); }

/* Write Functions */

// write a page to disk using absolute position
RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {

    RC check = RC_OK;

    // pageNum out of bounds
    if (pageNum < 0 || pageNum > (*fHandle).totalNumPages) return RC_WRITE_FAILED;

    // pageNum is within bounds
    file = fopen(fHandle->fileName, "r+");
    if (file == NULL) return RC_FILE_NOT_FOUND; // error writing to file
    
    if(pageNum !=0){
        fHandle->curPagePos = PAGE_SIZE * pageNum; // store current page position
        fclose(file); // close file
        writeCurrentBlock(fHandle, memPage); // write current block
        return RC_OK;
    }

    if (fseek(file, (PAGE_SIZE * pageNum), SEEK_SET) != 0) return RC_SEEK_FAILED;

    for (int i = 0; i < PAGE_SIZE; i++) {
        if (feof(file) == 1) appendEmptyBlock(fHandle); // end of file so append new empty block
        fputc(memPage[i], file); // write char
    }

    fHandle->curPagePos = ftell(file); // store current page position
    check = fclose(file); // close file

    return check; // write block successful
}

// write a page to disk using current position
RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) {

    file = fopen(fHandle->fileName, "r+"); // open file for r+w
    if (file == NULL) return RC_FILE_NOT_FOUND; // error writing block
    appendEmptyBlock(fHandle); // add space for new page data
    if (fseek(file, fHandle->curPagePos, SEEK_SET) != 0) return RC_SEEK_FAILED; // find page or return error
    fwrite(memPage, sizeof(char), strlen(memPage), file); // write page to file
    fHandle->curPagePos = ftell(file); // store current page position
    fclose(file); // close file

    return RC_OK; // writing current block to file successful
}

// increase the number of pages in the file by one
// new last page is zero bytes
RC appendEmptyBlock (SM_FileHandle *fHandle) {

    // could not find file, return failure
    if (fseek(file, 0, SEEK_END) != 0) return RC_SEEK_FAILED;    

    // create an empty page filled with zero bytes
    SM_PageHandle emptyPage = calloc(PAGE_SIZE, sizeof(char));
    fwrite(emptyPage, sizeof(char), PAGE_SIZE, file);
    free(emptyPage); // free used memory
    fHandle->totalNumPages++; // increase total num pages

    return RC_OK; // appending empty block succesful
}

// if the file has less than numberOfPages pages then increases the size of numberOfPages
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle) {
    
    RC check = RC_OK;

    file = fopen(fHandle->fileName, "a"); // append mode - open file to add data to eof
    if (file == NULL) return RC_FILE_NOT_FOUND; // file not found error
        
    // need more space, append empty block
    if(fHandle->totalNumPages<numberOfPages) for(int i=0;i<numberOfPages-(fHandle->totalNumPages);i++) appendEmptyBlock(fHandle);

    check = fclose(file); // close file

    return check;
}