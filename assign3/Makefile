.PHONY: all clean
all: compile run

compile: dberror.c storage_mgr.c buffer_mgr_stat.c buffer_mgr.c rm_serializer.c expr.c get_attr.c set_attr.c create_table.c record_mgr.c test_assign3_1_V2.c
	gcc -o test_assign3 test_assign3_1_V2.c dberror.c storage_mgr.c buffer_mgr_stat.c buffer_mgr.c expr.c get_attr.c set_attr.c create_table.c rm_serializer.c record_mgr.c

run:
	./test_assign3

clean:
	rm test_assign3
