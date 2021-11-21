#ifndef CREATE_TABLE_H
#define CREATE_TABLE_H

#include "dberror.h"
#include "expr.h"
#include "tables.h"
#include "storage_mgr.h"

void runStorageOps(char *name, char *page_content);
void setSchemaLayout(Schema *schema, char *page, int record_size);

#endif