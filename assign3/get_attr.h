#ifndef GET_ATTR_H
#define GET_ATTR_H

#include "dberror.h"
#include "expr.h"
#include "tables.h"

// get location of attribute from schema by using attribute number
char* getDataPointer (Record *record, Schema *schema, int attrNum);
Value* getAttrString (Schema *schema, int attrNum, Value *value, char* dataPtr);
Value* getAttrInt (Schema *schema, int attrNum, Value *value, char* dataPtr);
Value* getAttrFloat (Schema *schema, int attrNum, Value *value, char* dataPtr);
Value* getAttrBool (Schema *schema, int attrNum, Value *value, char* dataPtr);

#endif // GET_ATTR_H