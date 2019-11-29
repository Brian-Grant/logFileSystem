#include "HardDrive.h"

HardDrive::HardDrive() {
	this->writeFiles();
}

void HardDrive::writeFiles() {
	char zero = '0';
	int numFiles = 64;
	int fileLength = 1048576;
	//int fileLength = 100;
	int fd;
	if (mkdir("DRIVE", 0777) == -1) {
		cout << "Directory already exists." << endl;
	} else {
		cout << "Directory created" << endl;
		for (int i = 0; i < numFiles; i++) {
			char *fn = (char*)malloc(256);
			string copyStr = "DRIVE/SEGMENT"+to_string(i);
			for (int j = 0; j < int(copyStr.length()); j++) {
				fn[j] = copyStr[j];
			}
			fn[int(copyStr.length())] = '\0';

			fd = open(fn, O_CREAT | O_WRONLY, 0777);
			for (int j = 0; j < fileLength; j++) {
				write(fd, &zero, sizeof(zero));
			}
			close(fd);
			free(fn);
		}
		
		int crInitializer = -1;
		char liveInit = 0; // 0 == avaliable
		fd = open("DRIVE/CHECKPOINT_REGION", O_CREAT | O_WRONLY, 0777);
		for(int i = 0; i < 40; i++){
			write(fd, &crInitializer, sizeof(crInitializer));
		}
		for(int i = 0; i < 64; i++){
			write(fd, &liveInit, sizeof(liveInit));
		}
		
		//----------------------------------------------------------------------
		
	
		//----------------------------------------------------------------------	
		
				
		close(fd);
		fd = open("DRIVE/FILE_NAME_MAP", O_CREAT | O_WRONLY, 0777);
		close(fd);
		// write to fnm for debugging 
/*		ofstream out("DRIVE/FILE_NAME_MAP");
		if(out.is_open()){
			cout << "writing to fnm" << endl;			
			int num = 69;			
			out << "FiLeNaMe.txt" << endl;
			out << num << endl;
			out.close();
		}
		else{
			cout << "error" << endl;
		}
*/
	}
}
