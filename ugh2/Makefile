.PHONY: all clean
all: compile1 run1

compile1: dberror.c storage_mgr.c buffer_mgr_stat.c buffer_mgr.c test_assign2_1.c
	gcc -o test_assign1 test_assign2_1.c dberror.c storage_mgr.c buffer_mgr_stat.c buffer_mgr.c

run1:
	./test_assign1

clean:
	rm test_assign1
