scheduler: linked_list.o scheduler.o
	gcc -o scheduler scheduler.o linked_list.o -pthread
linked_list.o: linked_list.c linked_list.h
	gcc -c linked_list.c
scheduler.o: linked_list.h scheduler.c
	gcc -c scheduler.c
clean:
	rm *.o cpu *~
