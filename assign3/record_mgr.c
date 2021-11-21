#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "record_mgr.h"
#include "get_attr.h"
#include "set_attr.h"
#include "create_table.h"

typedef struct RM_Manager
{
	int num_records, free_page_idx, scan_count;
	BM_PageHandle page_handle;
	BM_BufferPool buffer_pool;
	RID rid;
	Expr *cond;
} RM_Manager;

RM_Manager *RM; // global RM represents record manager to handle tables with a fixed schema. 
const int RECORD_SIZE_BYTES = 20; // fixed record size

// table and manager
RC initRecordManager (void *mgmtData) {
	initStorageManager(); // initializing storage manager for page files
	return RC_OK; // initRecordManager successful
}

// shutdown record manager
RC shutdownRecordManager () {
	free(RM = NULL); // set RM to null before freeing it
	return RC_OK; // shutdownRecordManager successful
}

RC createTable (char *name, Schema *schema) {

	// storing the schema layout onto the page
	char page_content[PAGE_SIZE];
	char *page = page_content;
	setSchemaLayout(schema, page, RECORD_SIZE_BYTES);

	// initializing record manager and buffer pool. buffer pool is stored in record manager
	RM = (RM_Manager *) malloc(sizeof(RM_Manager));
	initBufferPool(&RM->buffer_pool, name, 1000, RS_LRU, NULL); // Note: 1000 default number of pages

	// creating the page file for the schema and table
	runStorageOps(name, page_content);

	return RC_OK; // createTable successful
}

RC openTable (RM_TableData *rel, char *name) {

	// pin page to change page
	pinPage(&RM->buffer_pool, &RM->page_handle, 0);

	SM_PageHandle page_handle;
	int num_attr;
	page_handle = (char *) RM->page_handle.data;
	RM->num_records = *(int *) page_handle;
	page_handle += 4;
	RM->free_page_idx = *(int *) page_handle;
	page_handle += 4;
	num_attr = *(int *) page_handle;
	page_handle += 4;

	Schema *temp_schema = (Schema *) malloc(sizeof(Schema));
	temp_schema->numAttr = num_attr;
	temp_schema->attrNames = (char **) malloc(sizeof(char *) * num_attr);
	temp_schema->dataTypes = (DataType *) malloc(sizeof(DataType) * num_attr);
	temp_schema->typeLength = (int *) malloc(sizeof(int) *num_attr);

	for (int i=0; i<temp_schema->numAttr; i++) {
		temp_schema->attrNames[i] = (char *) malloc(RECORD_SIZE_BYTES);
		strncpy(temp_schema->attrNames[i], page_handle, RECORD_SIZE_BYTES);
		page_handle += RECORD_SIZE_BYTES;

		temp_schema->dataTypes[i] = *(int *) page_handle;
		page_handle += 4;

		temp_schema->typeLength[i] = *(int *) page_handle;
		page_handle += 4;
	}

	// retrieving the schema from the page file and storing it in the RM_TableData
	rel->name = name;
	rel->mgmtData = (void *) RM;
	rel->schema = temp_schema;

	// unpin when finished changing
	unpinPage(&RM->buffer_pool, &RM->page_handle);

	return RC_OK; // openTable successful
}

RC closeTable (RM_TableData *rel) {
	// close table by flushing buffer pool
	RM_Manager *local_RM = (RM_Manager *) rel->mgmtData;
	shutdownBufferPool(&local_RM->buffer_pool);
	return RC_OK; // closeTable successful
}

RC deleteTable (char *name) {
	// delete table by destroy page file. Page file is stored in data pool
	destroyPageFile(name);
	return RC_OK; // deleteTable successful
}

int getNumTuples (RM_TableData *rel) {
	// get number of record in table
	RM_Manager *local_RM = (RM_Manager *) rel->mgmtData;
	return local_RM->num_records;
}

int next_slot(int record_size, char *page_content) {
	// finds the next free slot within the page layout

	for (int i = 0; i < (PAGE_SIZE / record_size); i++) if (page_content[i * record_size] != '@') return i;
	return -1; // could not find next free slot
}

// handling records in a table
RC insertRecord (RM_TableData *rel, Record *record) {
	// insert records into pagefile

	char *holder;
	char *page_content;
	RM_Manager *RM = (RM_Manager *) rel->mgmtData;
	RID *rid = &record->id; // get record id
	int size = getRecordSize(rel->schema); 

	rid->page = RM->free_page_idx; // set current page as the available page

	// pin to change pagefile
	pinPage(&RM->buffer_pool, &RM->page_handle, rid->page);

	holder = RM->page_handle.data; // set variable as the very first location of data of record

	// find the next free slot to insert the new record
	rid->slot = next_slot(size, holder);

	while (rid->slot == -1) {

		unpinPage(&RM->buffer_pool, &RM->page_handle);
		rid->page++;
		pinPage(&RM->buffer_pool, &RM->page_handle, rid->page);
		holder = RM->page_handle.data;
		rid->slot = next_slot(size, holder);
	}

	page_content = holder;
	page_content += (rid->slot * size);
	*page_content = '@'; // '@' means it's a empty record

	memcpy(++page_content, record->data + 1, size - 1);

	// unpin when finish inserting record into page file 
	// mark dirty because of change to page
	markDirty(&RM->buffer_pool, &RM->page_handle);
	unpinPage(&RM->buffer_pool, &RM->page_handle);
	RM->num_records++;
	pinPage(&RM->buffer_pool, &RM->page_handle, 0);

	return RC_OK; // insertRecord successful
}

RC deleteRecord (RM_TableData *rel, RID id) {
	char *page_content;
	RM_Manager *RM = (RM_Manager *) rel->mgmtData;

	//pin to change pageFile
	pinPage(&RM->buffer_pool, &RM->page_handle, id.page);

	RM->free_page_idx = id.page;

	page_content = RM->page_handle.data; // set variable as the very first location of data of record

	page_content += (id.slot * getRecordSize(rel->schema)); // reset the current location of the data

	*page_content = '!'; // '!' means it's a deleted record

	//mark dirty because of change to page
	//unpin when finish deleting record from pageFile
	markDirty(&RM->buffer_pool, &RM->page_handle);
	unpinPage(&RM->buffer_pool, &RM->page_handle);

	return RC_OK; // deleteRecord successful
}

RC updateRecord (RM_TableData *rel, Record *record) {
	char *page_content;
	RM_Manager *RM = (RM_Manager *) rel->mgmtData;
	RID id = record->id; // set variable as the id of record

	//pin to change pageFile
	pinPage(&RM->buffer_pool, & RM->page_handle, record->id.page);

	page_content = RM->page_handle.data; // set variable as the very first location of data of record
	page_content += (id.slot * getRecordSize(rel->schema)); // reset the current location of the data
	*page_content = '@'; // '@' means it's a empty record
	memcpy(++page_content, record->data + 1, getRecordSize(rel->schema) - 1);

	//mark dirty because of change to page
	//unpin when finish changing record from pageFile
	markDirty(&RM->buffer_pool, &RM->page_handle);
	unpinPage(&RM->buffer_pool, &RM->page_handle);

	return RC_OK; // updateRecord successful
}

RC getRecord (RM_TableData *rel, RID id, Record *record) {
	// get the target record from pageFile
	char *page_content;
	char *temp_content;
	RM_Manager *RM = (RM_Manager *) rel->mgmtData;

	//pin page to work on it
	pinPage(&RM->buffer_pool, &RM->page_handle, id.page);

	page_content = RM->page_handle.data; // set variable as the very first location of data of record 
	page_content += (id.slot * getRecordSize(rel->schema)); // reset the current location of the data

	if (*page_content == '@'){ // get record data
		record->id = id;
		temp_content = record->data;
		memcpy(++temp_content, page_content + 1, getRecordSize(rel->schema) - 1);
	}
	else return RC_RM_NO_TUPLE_WITH_GIVEN_RID;

	//unpin when done
	unpinPage(&RM->buffer_pool, &RM->page_handle);

	return RC_OK; // getRecord successful

}

// scans
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond) {
	RM_Manager *RM_scan;
	RM_Manager *RM_table;

	// check if there is scan condition
	if (cond == NULL) return RC_SCAN_CONDITION_NOT_FOUND;

	// open table for scan
	openTable(rel, "#temp#");
	RM_scan = (RM_Manager *) malloc (sizeof(RM_Manager));
	scan->mgmtData = (void *) RM_scan;
	scan->rel = rel;

	// Note: skipping page 0 since it stores infomation about the schema and not the records
	RM_table = (void *)rel->mgmtData;
	RM_table->num_records = RECORD_SIZE_BYTES;
	RM_scan->cond = cond, RM_scan->rid.page = 1, RM_scan->scan_count = 0, RM_scan->rid.slot = 0;

	return RC_OK; // startScan successful
}

RC next (RM_ScanHandle *scan, Record *record) {
	
	RM_Manager *RM_scan = scan->mgmtData;
	RM_Manager *RM_table = scan->rel->mgmtData;
	Schema *schema = scan->rel->schema;
	char *data;
	char *page_content;
	int size = getRecordSize(schema), temp_scan_count = RM_scan->scan_count;
	Value *result = (Value *) malloc(sizeof(Value));
	
	// check for scan condition
	if (RM_scan->cond == NULL) return RC_SCAN_CONDITION_NOT_FOUND;

	if (RM_table->num_records == 0) return RC_RM_NO_MORE_TUPLES;

	while (temp_scan_count <= RM_table->num_records) {

		if (temp_scan_count <= 0) RM_scan->rid.page = 1, RM_scan->rid.slot = 0;

		else {
			RM_scan->rid.slot++;
			if (RM_scan->rid.slot >= (PAGE_SIZE/size)) RM_scan->rid.slot = 0, RM_scan->rid.page++;
		}
		
		//pin to read
		pinPage(&RM_table->buffer_pool, &RM_scan->page_handle, RM_scan->rid.page);

		data = RM_scan->page_handle.data;
		data += (RM_scan->rid.slot * size);
		record->id.slot = RM_scan->rid.slot, record->id.page = RM_scan->rid.page;

		page_content = record->data;
		*page_content = '!';
		memcpy(++page_content, data + 1, size - 1);

		RM_scan->scan_count++, temp_scan_count++;

		//evaluate record with condition
		evalExpr(record, schema, RM_scan->cond, &result);

		if (result->v.boolV == TRUE) {
			unpinPage(&RM_table->buffer_pool, &RM_scan->page_handle);
			return RC_OK; // next successful
		}
	}

	// reset scanner
	RM_scan->rid.page = 1, RM_scan->scan_count = 0, RM_scan->rid.slot = 0;

	// unpin page when finish reading
	unpinPage(&RM_table->buffer_pool, &RM_scan->page_handle);

	return RC_RM_NO_MORE_TUPLES; // initRecordManager successful
}

RC closeScan (RM_ScanHandle *scan) {
	RM_Manager *RM = scan->rel->mgmtData;
	RM_Manager *RM_scan = scan->mgmtData;

	if (RM_scan->scan_count > 0) {
		
		// unpin page to finish reading
		unpinPage(&RM->buffer_pool, &RM_scan->page_handle);
		
		// reset scanner
		RM_scan->rid.page = 1, RM_scan->scan_count = 0, RM_scan->rid.slot = 0;
	}
	
	// free scan manager
	free(scan->mgmtData=NULL);

	return RC_OK; // closeScan successful
}

// dealing with schemas
int getRecordSize (Schema *schema) {
	int record_size = 1; // initialize variable with 1

	for (int i = 0; i < schema->numAttr; i++) { // get size using for loop from the very first to attrNum
		
		// when data type is DT_FLOAT
		if (schema->dataTypes[i] == DT_FLOAT) record_size += sizeof(float); // add the float size to the variable
		// when data type is DT_BOOL
		else if (schema->dataTypes[i] == DT_BOOL) record_size += sizeof(bool); // add the bool size to the variable
		// when data type is DT_INT
		else if (schema->dataTypes[i] == DT_INT) record_size += sizeof(int); // add the int size to the variable
		// when data type is DT_STRING
		else if (schema->dataTypes[i] == DT_STRING) record_size += schema->typeLength[i]; // add the total length of the string to the variable
	}
	
	return record_size;
}
// createSchema creates a schema
Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys) {

	Schema *schema_temp = (Schema *) malloc(sizeof(Schema)); // malloc schema
	
	// put all the given values into variable
	schema_temp->dataTypes = dataTypes;
	schema_temp->keyAttrs = keys;
	schema_temp->numAttr = numAttr;
	schema_temp->keySize = keySize;
	schema_temp->typeLength = typeLength;
	schema_temp->attrNames = attrNames;

	return schema_temp; //return schema
}
// freeSchema is to free the schema
RC freeSchema (Schema *schema) {
	free(schema->attrNames=NULL);
	free(schema->dataTypes=NULL);
	free(schema->typeLength=NULL);
	free(schema->keyAttrs=NULL);
	free(schema=NULL); // free the schema
	return RC_OK; // freeSchema successful
}

// dealing with records and attribute values
// createRecord creates an empty record and return that pointer back to the user
RC createRecord (Record **record, Schema *schema) {

	Record *newRecord = (Record *) malloc(sizeof(Record)); // malloc a Record structure
	int recordSize = getRecordSize(schema); //get the record size

	newRecord->data = (char *) malloc(recordSize); // malloc size of recordSize to variable
	newRecord->id.page = newRecord->id.slot = -1; // set the page and slot position as -1, which indicates it is a new record

	char *dataptr = newRecord->data; // set variable as the very first location of data of record
	*dataptr = '!';

	*(++dataptr) = '\0'; // add '\0' at the end of the record

	*record = newRecord; // set variable as the malloced Record structure having data

	return RC_OK; // createRecord successful
}

// freeRecord is to free record
RC freeRecord (Record *record) {
	free(record->data=NULL);
	free(record=NULL); // free record
	return RC_OK; // freeRecord successful
}

// getAttr is to get attributes from the record by using attribute number based on the data type
RC getAttr (Record *record, Schema *schema, int attrNum, Value **value) {

	char* page_content = getDataPointer(record, schema, attrNum);
	Value *attr_temp = (Value *) malloc(sizeof(Value)); // malloc a Value structure

	schema->dataTypes[attrNum] = (attrNum == 1) ? 1 : schema->dataTypes[attrNum]; // get the data types from schema

	if (schema->dataTypes[attrNum] == DT_FLOAT) getAttrFloat(schema, attrNum, attr_temp, page_content);
	if (schema->dataTypes[attrNum] == DT_BOOL) getAttrBool(schema, attrNum, attr_temp, page_content);
	if (schema->dataTypes[attrNum] == DT_STRING) getAttrString(schema, attrNum, attr_temp, page_content);
	if (schema->dataTypes[attrNum] == DT_INT) getAttrInt(schema, attrNum, attr_temp, page_content);

	*value = attr_temp; // set variable as the malloced Value structure having data
	return RC_OK; // getAttr successful
}

// setAttr is to set attributes to the record by using attribute number based on the data type
RC setAttr (Record *record, Schema *schema, int attrNum, Value *value) {

	char *page_content = getDataPointer(record, schema, attrNum);

	if (schema->dataTypes[attrNum] == DT_FLOAT) page_content = setFloat(schema, attrNum, value, page_content);
	else if (schema->dataTypes[attrNum] == DT_BOOL) page_content = setBool(schema, attrNum, value, page_content);
	else if (schema->dataTypes[attrNum] == DT_STRING) page_content = setString(schema, attrNum, value, page_content);
	else if (schema->dataTypes[attrNum] == DT_INT) page_content = setInt(schema, attrNum, value, page_content);

	return RC_OK; // setAttr successful
}