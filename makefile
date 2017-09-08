all: clean proj4

proj4: proj4.o
	g++ proj4.o -o proj4 -pthread
	
proj4.o: proj4.cpp
	g++ -c proj4.cpp -pthread

clean: 
	rm *.o proj4 -f
