all:	HardDrive

HardDrive:	HardDrive.o driver.o LFS.o
	g++ HardDrive.o driver.o LFS.o -o HardDrive

LFS.o:	LFS.cpp
	g++ -c -g -Wall LFS.cpp

HardDrive.o:	HardDrive.cpp
	g++ -c -g -Wall HardDrive.cpp

driver.o:	driver.cpp
	g++ -c -g -Wall driver.cpp

clean:
	rm -f *.o HardDrive

cleandir:
	rm -rf DRIVE

test:
	./HardDrive

val:
	valgrind ./HardDrive
