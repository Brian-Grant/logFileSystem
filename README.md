logFileSystem

Brian Grant

Compile with make, run with ./HardDrive

On the first run, this program will create a hard drive which constitutes the
	file system. This takes a bit of time. Subsequent runs are much faster 
	because it does not need to make the hard drive. 

This repo contains a few toy file examples to experiment with:
	fun.txt
	in.txt
	input.txt
	input2.txt
	test.txt
	test2.txt

run "make clean" to delete object files and executables 
run "make cleandir" to delete the hard drive

When the program is running, run the following commands:

Shutdown - shuts down the file system and writes anything in ram to disk
	
clean - implements the cleaning of segments which contain invalid data
	to increase space utilization

import <filename> <lfs_filename> - causes the linux file named <filename>
	to be read and stored in the lfs. The file will be saved as 
	<lfs_filename> in the lfs file system

remove <lfs_filename> - removes specified file from the system

list - Lists all names of the files in the system and the size of each
	file

restart - Saves all data currently in ram to the disk and restarts the
	system

cat <lfs_filename> - Displays the contents of <lfs_filename> on the screen 

display <lfs_filename> <how_many> <start> - Read and display <how_many> 
	bytes from the file named <lfs_filename> beginning at logical byte 
	<start>. If <how_many> exceeds the end of the file, only up to and 
	including the last byte is displayed on the screen
	
overwrite <lfs_filename> <how_many> <start> <c> - Writes <how_many>
	copies of the character <c> into file <lfs_filename> beginning at
	byte <start>. If <start> + <how_many> exceeds the current file size
	this command will increase the file size appropriately 

I noticed anomalies here and there. I plan on working out the kinks in the 
future.
