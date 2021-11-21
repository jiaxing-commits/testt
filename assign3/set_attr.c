#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dberror.h"
#include "expr.h"
#include "tables.h"

void setString(Schema *schema, int attrNum, Value *value, char *dataptr) {
	int length = schema->typeLength[attrNum]; // set variable as the typeLength of attribute
	strncpy(dataptr, value->v.stringV, length); // copy string from variable with amount of the typeLength to data
	dataptr += schema->typeLength[attrNum]; // reset the current location of the data array by adding the size of typeLength of the attribute
}

void setInt(Schema *schema, int attrNum, Value *value, char *dataptr) {
	*(int *) dataptr = value->v.intV; // set data with variable
	dataptr += sizeof(int); // reset the current location of the data array by adding the size of typeLength of the attribute
}

void setFloat(Schema *schema, int attrNum, Value *value, char *dataptr) {
	*(float *) dataptr = value->v.floatV; // set data with variable
	dataptr += sizeof(float); // reset the current location of the data array by adding the size of typeLength of the attribute
}

void setBool(Schema *schema, int attrNum, Value *value, char *dataptr) {
	*(bool *) dataptr = value->v.boolV; // set data with variable
	dataptr += sizeof(bool); // reset the current location of the data array by adding the size of typeLength of the attribute
}
