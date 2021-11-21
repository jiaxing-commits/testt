#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dberror.h"
#include "expr.h"
#include "tables.h"
#include "storage_mgr.h"

void runStorageOps(char *name, char* page_content) {
	SM_FileHandle file;
    createPageFile(name);
	openPageFile(name, &file);
	writeBlock(0, &file, page_content);
    closePageFile(&file);
}

char *setSchemaLayout(Schema *schema, char *page, int record_size) {
    *(int *) page = 0;
	page += 4; // Note: 4 is size of int
	*(int *) page = 1;
	page += 4;
	*(int *) page = schema->numAttr;
	page += 4;
	*(int *) page = schema->keySize;
	page += 4;
	for (int i = 0; i < schema->numAttr; i++) {
		strncpy(page, schema->attrNames[i], record_size);
		page += record_size;
		*(int *) page = (int) schema->dataTypes[i];
		page += 4;
		*(int *) page = (int) schema->typeLength[i];
		page += 4;
	}

}