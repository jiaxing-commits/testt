#ifndef SET_ATTR_H
#define SET_ATTR_H

#include "dberror.h"
#include "expr.h"
#include "tables.h"

// get location of attribute from schema by using attribute number
//char* getDataPointer (Record *record, Schema *schema, int attrNum);
void* setString (Schema *schema, int attrNum, Value *value, char* dataptr);
void* setInt (Schema *schema, int attrNum, Value *value, char* dataptr);
void* setFloat (Schema *schema, int attrNum, Value *value, char* dataptr);
void* setBool (Schema *schema, int attrNum, Value *value, char* dataptr);

#endif // SET_ATTR_H