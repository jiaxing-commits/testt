Group 3 - The Best Group
Teammates: Juseung Lee, Kunal Patil, Jiaxing Wu

#This file records the implementataion of record manager for assignment3#
-----------------------------------------------------------------------------------------------
Description: The purpose of the record manager is to handle tables with a fixed schema. The 
record manager can access the pages of the file through the buffer manager which implemented
in PA2.

There are five type of functions in the record manager:
  * Table and Record Manager functions
  * Record functions
  * Scan functions
  * Schema functions
  * Attribute functions

* Table and Record Manager functions
  : These functions are used to initialize and shutdown a record manager. In addition, these 
  functions are able to create, open, and close a table. RM_TableData struct can be used in
  order to ineracte with the table.

* Record functions
  : These functions are used to retrieve a record with a certain RID, to delete a record 
  with a certain RID, to insert a new record, and to update an existing record with new values.

* Scan functions
  : These functions are used to retrieve all tuples from a table that fulfill a certain 
  condition.

* Schema functions
  : These functions are used to return the size in bytes of records for a given schema and 
  create a new schema.

* Attribute functions
  : These functions are used to get or set the attribute values of a record and create a new 
  record for a given schema.

-----------------------------------------------------------------------------------------------
Testing and Running: All of the tests are administered through the test_assign3_1.c file.

To compile and run the tests, type:
make

To clean up after tests, type:
make clean