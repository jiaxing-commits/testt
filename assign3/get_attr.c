#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dberror.h"
#include "expr.h"
#include "tables.h"

// get location of attribute from schema by using attribute number
RC attrOffset (Schema *schema, int attrNum, int *result) {

	*result = 1; // initialize variable
	for (int i = 0; i < attrNum; i++) { // get location using for loop from the very first to attrNum
		// when data type is DT_INT add the int size to the variable
		if (schema->dataTypes[i] == DT_INT) *result += sizeof(int);
		// when data type is DT_STRING add the total length of the string to the variable
		else if (schema->dataTypes[i] == DT_STRING) *result += schema->typeLength[i];
		// when data type is DT_FLOAT add the flozt size to the variable
		else if (schema->dataTypes[i] == DT_FLOAT) *result += sizeof(float);
		// when data type is DT_BOOL add the boolean size to the variable
		else if (schema->dataTypes[i] == DT_BOOL) *result += sizeof(bool);
	}

	return RC_OK;
}

// setAttr is to set attributes to the record by using attribute number based on the data type
char* getDataPointer(Record *record, Schema *schema, int attrNum) {
	int offset = 0;
	attrOffset(schema, attrNum, &offset); // get location of attribute from given schema by using given attribute number
	char* dataPtr = record->data; // get the data array from record
	return dataPtr += offset; // set the current location of the data array
}

void* getAttrString(Schema *schema, int attrNum, Value *attr, char *dataPtr) {
	int length = schema->typeLength[attrNum]; // set variable as the typeLength of attribute
	attr->v.stringV = (char *) malloc(length + 1); // malloc a char structure
	strncpy(attr->v.stringV, dataPtr, length); // copy string from data with amount of the typeLength to variable
	attr->dt = DT_STRING, attr->v.stringV[length] = '\0'; // add null value to the end of variable and set data type as DT_STRING
}

void* getAttrInt(Schema *schema, int attrNum, Value *attr, char *dataPtr) {
	int temp = 0; // initialize variable
	memcpy(&temp, dataPtr, sizeof(int)); // copy memory from data with amount of the int size to variable
	attr->dt = DT_INT, attr->v.intV = temp; // set variable as copied memory and set data type as DT_INT
}

void* getAttrFloat(Schema *schema, int attrNum, Value *attr, char *dataPtr) {
	float temp;
	memcpy(&temp, dataPtr, sizeof(float)); // copy memory from data with amount of the float size to variable
	attr->dt = DT_FLOAT, attr->v.floatV = temp; // set variable as copied memory and set data type as DT_FLOAT
}

void* getAttrBool(Schema *schema, int attrNum, Value *attr, char *dataPtr) {
	bool temp;
	memcpy(&temp, dataPtr, sizeof(bool)); // copy memory from data with amount of the boolean size to variable
	attr->dt = DT_BOOL, attr->v.boolV = temp; // set variable as copied memory and the data type as DT_BOOL
}