ex4.o: Router.c
	$gcc Router.c -g -o ex4 -lpthread
clean:
	rm -f *.o
