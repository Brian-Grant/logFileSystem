#include "LFS.h"



LFS::LFS(){
	this->loadCR();
	this->loadFNM();
	this->initIMap();
	int seg = this->findCleanSeg();
	if(seg == -1){
		//	clean
	}
	this->segPtr = seg;	
	this->blockPtr = 8;
	this->loadIMap();
	this->zeroSsb();
}

LFS::~LFS(){

}



void LFS::loadFNM(){
	int lineCtr = 0;	
	ifstream fnm("DRIVE/FILE_NAME_MAP");
	string line;
	string name;
	string nnstr;
	int nodeNum;
	if(fnm.is_open()){		
		while(getline(fnm, line)){
			if(lineCtr%2==0){ // filename
				name = line;
			}
			else{ // nodeNumber
				nnstr = line;
				nodeNum = stoi(nnstr);
				this->fnm.insert(pair<string, int>(name, nodeNum));
			}
			lineCtr++;
		}
		fnm.close();
	}
}


void LFS::loadCR(){
	int fd = open("DRIVE/CHECKPOINT_REGION", O_RDONLY, 0777);
	for(int i = 0; i < 40; i++){
		read(fd, &this->checkpoint[i], 4);
	}
	for(int i = 0; i < 64; i++){
		read(fd, &this->liveness[i], 1);
	}
}

// returns -1 if we need to clean
int LFS::findCleanSeg(){
	int live = -1;	
	for(int i = 0; i < 64; i++){
		if(this->liveness[i] == 0){
			live = i;
			//cout << "live " << live << endl;
			//this->liveness[live] = 1;			
			break;
		}
	}
	this->liveness[live] = 1;
	return live;
}

void LFS::initIMap(){
	for(int i = 0; i < 40; i++){
		for(int j = 0; j < 256; j++){	
			this->iMap[i].imap.locations[j] = -1;
		}
	}
}

void LFS::loadIMap(){
	int entry;
	int segNum;
	int offset;
	int pieceCounter = 0;	
	int fd;
	string copyStr;
	for(int i = 0; i < 40; i++){ // for each entry in the checkpoint
		entry = this->checkpoint[i];
		if(entry != -1){ // if there is a valid entry
			segNum = entry / 1024; // locate imap piece by seg number
			offset = entry % 1024; // and offset
			
			// get name and convert to char*
			char *fn = (char*)malloc(256); 
			copyStr = "DRIVE/SEGMENT"+to_string(segNum);
			for (int j = 0; j < int(copyStr.length()); j++) {
				fn[j] = copyStr[j];
			}
			fn[int(copyStr.length())] = '\0';
			// open, seek, read			
			fd = open(fn, O_RDWR, 0777); // open the file
			lseek (fd, (offset*1024), SEEK_CUR); // seek to offset of correct segment //TODO potential bug
			read(fd, &this->iMap[pieceCounter], 1024); // read in the piece
			pieceCounter++;	
		}
	}
	//cout << "In loadImap" << endl;
	//this->_seeimap(); // debug
}	

void LFS::import(string filename, string lfs_filename) {
	//cout << "beginning import" << endl;	
	//this->_seessb();	
	//cout <<	"in import" << endl;
	//this->_seeliveness();	
	if (this->fnm.size() > 10240) {
		cout << "Reached max number of files." << endl;
	}
	char cfilename[128];
	char lfilename[128];
	memcpy(cfilename, filename.c_str(), filename.size()+1);
	memcpy(lfilename, lfs_filename.c_str(), lfs_filename.size()+1);
	lfilename[strlen(lfilename)] = '\0';	
	cfilename[strlen(cfilename)] = '\0';
	int fd = open(cfilename, O_RDONLY, 0777);
	int fileLength = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET); // move file ptr position to beginning of the file
	

	//Set number of data blocks 
	int numDataBlocks;
	if (fileLength%1024 != 0) {
		numDataBlocks = (fileLength/1024)+1;
	} else {
		numDataBlocks = fileLength/1024;
	}
	if(numDataBlocks > 128){
		cout << "file size exceeds maximum of 128k" << endl;
		return;
	}

	//read in file
	char content[fileLength-1]; //change to fileLength if we need null terminator
	read(fd, content, sizeof(content));
	close(fd);

	//Set up data blocks of file contents
	dataBlock dbArr[numDataBlocks];
	int ctr = 0;
	bool done = false;	
	for(int i = 0; i < numDataBlocks; i++){
		for(int j = 0; j < 1024; j++){
			dbArr[i].data[j] = content[ctr];
			ctr++;
			if(ctr == (fileLength-1)){
				done = true;
				break;
			}
		}
		if(done) break;
	}
	
	//Set up inode

	memcpy(this->inode.fileName, lfilename, sizeof(lfilename)); // set filename
	this->inode.size = fileLength;								// set size
	int nodeNumber = this->findFreeInode();				     // find free node

	int blockNum = this->segPtr*1024 + this->blockPtr; // find location of where
	for(int i = 0; i < numDataBlocks; i++){	// we are going to write to 
		//if(blockNum+2 < (64*1024)){ // if we did not reach the end of whole sys		
			//this->inode.dBlocks[i] = blockNum; // write direct block ptrs
		//}
		//else{
		//	cout << "Reached end of system" << endl;
		//	return;
		//}
		//blockNum++;
		this->inode.dBlocks[i] = blockNum++;
	}
	this->fnm.insert(pair<string, int>(lfs_filename, nodeNumber)); // update file name map

	int dataLocation = this->segPtr*1024 + this->blockPtr; // data bloc location
	int inodeLocation = dataLocation + numDataBlocks; // inode location after data
	int iMapLocation = inodeLocation + 1; // imap loc after inode
	int imappiece = nodeNumber / 256;
	int off = nodeNumber % 256; 
	this->iMap[imappiece].imap.locations[off] = inodeLocation;
	this->checkpoint[imappiece] = iMapLocation; 

	int freeSeg;	
	int nextSeg;
	int dataCtr = 0;
	bool segOverflow = false;
	string copyStr;
	char* fn = (char*)malloc(256);
	if(this->blockPtr == 1024){ // if some other func filled the segment but did not write it out like remove..maybe overwrite later
		segOverflow = true;
	}
	while(this->blockPtr < 1024 && dataCtr < numDataBlocks){ // write data blocks to the segment while there is still room
		memcpy(&this->segment[this->blockPtr], &dbArr[dataCtr], 1024);	
		//write in ssb nodeNumber followed by dataCtr
		// ssb stuuff ---------------------------------------------------------
		int ssbBlock = (this->blockPtr-8)/128;
		int ssbOff = ((this->blockPtr-8)%128)*2;
		this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff] = nodeNumber;
		this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff+1] = dataCtr;
		// end ssb ------------------------------------------------------------		
		dataCtr++;
		this->blockPtr++;
		if(this->blockPtr == 1024){
			segOverflow = true;
		}
	}
	if(segOverflow){  
		freeSeg = this->segPtr;
		this->liveness[freeSeg] = 1;
		nextSeg = this->findCleanSeg();		
		if(nextSeg == -1){
			//clean
		}
		this->segPtr = nextSeg;
		copyStr = "DRIVE/SEGMENT"+to_string(freeSeg);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		fd = open(fn, O_WRONLY, 0777); // open the file
		write(fd, &this->segment, sizeof(this->segment));
		this->blockPtr = 8;
		this->zeroSsb();
		//write the remainder of the data to the fresh segment in ram
		while(this->blockPtr < 1024 && dataCtr < numDataBlocks){ 
			memcpy(&this->segment[this->blockPtr], &dbArr[dataCtr], 1024);	
			// ssb stuuff ---------------------------------------------------------
			int ssbBlock = (this->blockPtr-8)/128;
			int ssbOff = ((this->blockPtr-8)%128)*2;
			this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff] = nodeNumber;
			this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff+1] = dataCtr;
			// end ssb ------------------------------------------------------------				
			dataCtr++;
			this->blockPtr++;
		}
		close(fd);
	}
	memcpy(&this->segment[this->blockPtr], &this->inode, 1024);
	// ssb stuuff ---------------------------------------------------------
	int ssbBlock = (this->blockPtr-8)/128;
	int ssbOff = ((this->blockPtr-8)%128)*2;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff] = nodeNumber;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff+1] = -1;
	// end ssb ------------------------------------------------------------
	this->blockPtr++;
	if(this->blockPtr == 1024){	// do seg overflow stuff
		freeSeg = this->segPtr;
		this->liveness[freeSeg] = 1; //test
		nextSeg = this->findCleanSeg();	
		if(nextSeg == -1){
			//clean
		}
		this->segPtr = nextSeg;
		copyStr = "DRIVE/SEGMENT"+to_string(freeSeg);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		fd = open(fn, O_WRONLY, 0777); // open the file
		write(fd, &this->segment, sizeof(this->segment));
		
		this->blockPtr = 8;
		this->zeroSsb();
		//memcpy(&this->segment[this->blockPtr], &this->inode, 1024);
		close(fd);
	}
	memcpy(&this->segment[this->blockPtr], &this->iMap[imappiece] , 1024);
	// ssb stuuff ---------------------------------------------------------
	ssbBlock = (this->blockPtr-8)/128;
	ssbOff = ((this->blockPtr-8)%128)*2;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff] = -1;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff+1] = imappiece;
	// end ssb ------------------------------------------------------------
	this->blockPtr++;
	if(this->blockPtr == 1024){ // do seg overflow stuff
		freeSeg = this->segPtr;
		this->liveness[freeSeg] = 1; //test
		nextSeg = this->findCleanSeg();		
		if(nextSeg == -1){
			//clean
			
		}
		this->segPtr = nextSeg;
		copyStr = "DRIVE/SEGMENT"+to_string(freeSeg);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		fd = open(fn, O_WRONLY, 0777); // open the file
		write(fd, &this->segment, sizeof(this->segment));
		this->blockPtr = 8;
		this->zeroSsb();
		//memcpy(&this->segment[this->blockPtr], &this->iMap[imappiece] , 1024); 
		close(fd);
	}

	//cout << "At End of import" << endl;
	//this->_seeimap();
	
	// this function will never leave a segment full in ram
	//cout << "beginning import" << endl;	
	//this->_seessb();
}

// returns -1 if no free iNodes
int LFS::findFreeInode(){
	int idx = -1;
	bool bust = false;
	for(int i = 0; i < 40; i++){ // for each imap piece in ram
		for(int j = 0; j < 256; j++){	// for each entry in each piece
			if(this->iMap[i].imap.locations[j] == -1){ // if -1, iNode is free
				idx = (i*256)+j;
				bust = true;
				break;
			} 
		}
		if (bust) break;
	}
	//cout << "idx in findFreeInode" << idx << endl;	
	return idx;
}

//lists filename and file size
void LFS::listInfo() {
	map<string, int>::iterator it;
	block current;	
	for (it = this->fnm.begin(); it != this->fnm.end(); it++) {
		char * fn = (char *)malloc(256);
		for (int j = 0; j < int(it->first.length()); j++) {
			fn[j] = it->first[j];
		}
		fn[int(it->first.length())] = '\0';
		cout << it->first << ", ";
		int imapPartNum = it->second/256;
		int offset = it->second%256;
		int blockNum = iMap[imapPartNum].imap.locations[offset];
		int segNum = blockNum/1024;
		int segOffset = blockNum%1024;
		string copyStr;
		if(segNum == this->segPtr){
			printf("%d\n", segment[segOffset].inode.size);
		}
		else{
			copyStr = "DRIVE/SEGMENT"+to_string(segNum);
			for (int j = 0; j < int(copyStr.length()); j++) {
				fn[j] = copyStr[j];
			}
			fn[int(copyStr.length())] = '\0';
			int fd = open(fn, O_RDONLY, 0777);
			lseek (fd, (segOffset*1024), SEEK_SET); // seek to offset of correct segment //TODO potential bug
			read(fd, &current, 1024); // read in the piece
			printf("%d\n", current.inode.size);
		}
		free(fn);
	}
}

void LFS::writeFNM() {
	ofstream ofile("DRIVE/FILE_NAME_MAP");
	map<string, int>::iterator it;
	for (it = this->fnm.begin(); it != this->fnm.end(); it++) {
		//cout << "Writing " << it->first << ", " << it->second << endl;
		ofile << it->first << "\n";
		ofile << it->second << "\n";		
	}
	ofile.close();
}

void LFS::writeCR(){
	// populate the CR in ram	
	int fd = open("DRIVE/CHECKPOINT_REGION", O_WRONLY, 0777);
	//cout << "WRITING" << endl;
	for(int i = 0; i < 40; i++){
		write(fd, &this->checkpoint[i], 4);
		//printf("%d\n", this->checkpoint[i]);
	}

	for(int i = 0; i < 64; i++){
		write(fd, &this->liveness[i], 1);
		//printf("%d\n", this->liveness[i]);
	}
	close(fd);
}

void LFS::shutDown() {
	this->writeFNM();
	if(this->blockPtr > 8){	
		int fd;
		string copyStr;
		char* fn = (char*)malloc(256);
		copyStr = "DRIVE/SEGMENT"+to_string(this->segPtr);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		fd = open(fn, O_RDWR, 0777); // open the file
		write(fd, &this->segment, sizeof(this->segment));
		this->liveness[this->segPtr] = 1;
		free(fn);
		this->blockPtr = 8;
	}
	this->writeCR();
}

// find iNode num in fnm
// remove entry in the imap
// put that piece of the imap in the in-memory seg
// update the CR
// remove from fnm
void LFS::remove(string lfs_filename) {
	int iNodeNum = fnm.find(lfs_filename)->second; // get inode num
	fnm.erase(lfs_filename);	  					// remove inode from fnm
		
	int iMapPiece = iNodeNum/256;				
	int offset = iNodeNum%256;
	iMap[iMapPiece].imap.locations[offset] = -1;	// remove iNode from iMap
	if(this->blockPtr < 1024){
		memcpy(&this->segment[this->blockPtr], &iMap[iMapPiece], 1024);
		int blockNum = this->segPtr*1024 + this->blockPtr; 
		this->checkpoint[iMapPiece] = blockNum;
		this->blockPtr++;
	}							
}


void LFS::restart(){
	this->shutDown();
}


void LFS::cat(string lfs_filename){
	//cout << "in cat" << endl;
	block current;	
	char* fn = (char*)malloc(256);
	
	
	int iNodeNum = fnm.find(lfs_filename)->second;
	int imapPartNum = iNodeNum/256;
	int offset = iNodeNum%256;
	int blockNum = iMap[imapPartNum].imap.locations[offset];
	int segNum = blockNum/1024;
	int segOffset = blockNum%1024;
	string copyStr;
	if(segNum == this->segPtr){
		//printf("%d\n", segment[segOffset].inode.size);
		memcpy(&current, &this->segment[segOffset], sizeof(current));
	}
	else{
		copyStr = "DRIVE/SEGMENT"+to_string(segNum);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		int fd = open(fn, O_RDONLY, 0777);
		lseek (fd, (segOffset*1024), SEEK_SET); // seek to offset of correct segment //TODO potential bug
		read(fd, &current, 1024); // read in the piece
	}

	//cout << current.inode.fileName << endl;
	int fileSize = current.inode.size;
	int numBlocks;
	int lastBlockSize;
	if(fileSize%1024 != 0){
		numBlocks = (fileSize/1024)+1;
		lastBlockSize = fileSize%1024;
	}
	else{
		numBlocks = fileSize/1024;
		lastBlockSize = 1024;
	}
	//cout << numBlocks << endl;
	int blockLocation;
	int seg;
	int off;
	for(int i = 0; i < numBlocks; i++){
		blockLocation = current.inode.dBlocks[i];
		//cout << blockLocation << endl;
		seg = blockLocation/1024;
		off = blockLocation%1024;
		if(i < (numBlocks-1)){		
			this->printDataBlock(seg, off, 1024, 0);	
		}
		else{
			this->printDataBlock(seg, off, lastBlockSize, 0);
		}
	}
	cout << endl;

	
}

int LFS::printDataBlock(int segment, int offset, int numChar, int start){
	//cout << "in print" << numChar << endl;
	block temp;
	int fd;
	string copyStr;
	
	if(segment == this->segPtr){
		memcpy(&temp, &this->segment[offset], 1024);
	}
	else{
				
		char* fn = (char*)malloc(256);
		copyStr = "DRIVE/SEGMENT"+to_string(segment);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		fd = open(fn, O_RDONLY, 0777);
		lseek (fd, (offset*1024), SEEK_SET); // seek to offset of correct segment //TODO potential bug
		read(fd, &temp, 1024);
		close(fd);
	}
	int ctr = 0;
	//cout << "start " << start << endl;
	//cout << "numChar " << numChar << endl;
	for(int i = start; i < numChar; i++){
		ctr++;		
		printf("%c", temp.data.data[i]);
	}
	//cout << "ctr " << ctr << endl;
	return ctr;


	// get data block print the numChar
}

void LFS::display(string lfs_filename, int howMany, int start){
	//cout << "in display" << endl;
	block current;	
	char* fn = (char*)malloc(256);
	int iNodeNum = fnm.find(lfs_filename)->second;
	int imapPartNum = iNodeNum/256;
	int offset = iNodeNum%256;
	int blockNum = iMap[imapPartNum].imap.locations[offset];
	int segNum = blockNum/1024;
	int segOffset = blockNum%1024;
	string copyStr;
	if(segNum == this->segPtr){
		//printf("%d\n", segment[segOffset].inode.size);
		memcpy(&current, &this->segment[segOffset], sizeof(current));
	}
	else{
		copyStr = "DRIVE/SEGMENT"+to_string(segNum);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		int fd = open(fn, O_RDONLY, 0777);
		lseek (fd, (segOffset*1024), SEEK_SET); // seek to offset of correct segment //TODO potential bug
		read(fd, &current, 1024); // read in the piece
	}

	//cout << current.inode.fileName << endl;
	int fileSize = current.inode.size;
	int numBlocks;
	int lastBlockSize;
	if(fileSize%1024 != 0){
		numBlocks = (fileSize/1024)+1;
		lastBlockSize = fileSize%1024;
	}
	else{
		numBlocks = fileSize/1024;
		lastBlockSize = 1024;
	}
	
	if(start >= fileSize){ // it is >= because the last byte is bye filesize-1
		cout << "<start> exceeds file size" << endl;
		return;
	}
	int endBlock = start + howMany;
	int difference = 0;	
	int augmentedHowMany = howMany;	
	if(endBlock >= fileSize){
		difference = endBlock - fileSize;
		augmentedHowMany -= difference;
	}
	//cout << "fileSize " << fileSize << endl;
	//cout << "start " << start << endl;
	//cout << "augmentedHowMany " << augmentedHowMany << endl;
	//cout << numBlocks << endl;
		
	int blockLocation;
	int seg;
	int off;
	
	int remaining = howMany;
	int firstBlock = start/1024;
	int firstOff = start%1024;
	for(int i = firstBlock; i < numBlocks; i++){
		blockLocation = current.inode.dBlocks[i];
		//cout << blockLocation << endl;
		seg = blockLocation/1024;
		off = blockLocation%1024;
		if(remaining == 0) break;		
		if(i == firstBlock){
			int many = 1024-firstOff;
			if(remaining > many){			
				//cout << "d1" << endl;				
				remaining -= this->printDataBlock(seg, off, 1024, firstOff);	
			}			
			else{
				//cout << "d2" << endl;
				//cout << "firstOff " << firstOff << endl;				
				//cout << "Rem before " <<remaining << endl;				
				int end = remaining + firstOff;				
				remaining -= this->printDataBlock(seg, off, end, firstOff);
				//cout << "rem After " << remaining << endl;		
			}
		}
		if(i == (numBlocks-1)){
			if(remaining < lastBlockSize){				
				this->printDataBlock(seg, off, remaining, 0);
			}
			else{			
				this->printDataBlock(seg, off, lastBlockSize, 0);
			}			
		}		
			
		else{	// if we are not in  the first of last block	
			if(remaining > 1024){				
				//cout << "d3" << endl;				
				remaining -= this->printDataBlock(seg, off, 1024, 0);	
			} 
			else{
				//cout << "d4" << endl;	
				int end = remaining; //+ firstOff;				
				remaining -= this->printDataBlock(seg, off, end, 0);
			}
		}
	}
	cout << endl;

}

void LFS::overwrite(string lfs_filename, int howMany, int start, char c){
	if(howMany == 0) return;	
	//cout << "in overwrite" << endl;
	block current;	
	char* fn = (char*)malloc(256);
	int iNodeNum = fnm.find(lfs_filename)->second;
	int imapPartNum = iNodeNum/256;
	int offset = iNodeNum%256;
	int blockNum = iMap[imapPartNum].imap.locations[offset];
	int segNum = blockNum/1024;
	int segOffset = blockNum%1024;
	string copyStr;
	if(segNum == this->segPtr){
		//printf("%d\n", segment[segOffset].inode.size);
		memcpy(&current, &this->segment[segOffset], sizeof(current));
	}
	else{
		copyStr = "DRIVE/SEGMENT"+to_string(segNum);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		int fd = open(fn, O_RDONLY, 0777);
		lseek (fd, (segOffset*1024), SEEK_SET); // seek to offset of correct segment //TODO potential bug
		read(fd, &current, 1024); // read in the piece
	}

	//cout << current.inode.fileName << endl;
	int fileSize = current.inode.size;
	//int numBlocks;
	//int lastBlockSize;
	//if(fileSize%1024 != 0){
	//	numBlocks = (fileSize/1024)+1;
	//	lastBlockSize = fileSize%1024;
	//}
	//else{
	//	numBlocks = fileSize/1024;
	//	lastBlockSize = 1024;
	//}
	
	if(start >= fileSize){ // it is >= because the last byte is bye filesize-1
		cout << "<start> exceeds file size" << endl;
		return;
	}
	int endChar = start + howMany;
	int difference = 0;	
	int augmentedHowMany = howMany;	
	int maxFileSize = 131072;	
	if(endChar >= fileSize){ // if the last char we are writing to is past eof
		if(endChar > maxFileSize){ // if its past maxFileSize
			fileSize = maxFileSize; 
			difference = endChar - maxFileSize;
			augmentedHowMany -= difference; // write to max file size
		}
		else{
			fileSize = endChar;
		}
	} 
	current.inode.size = fileSize;
	
	int firstBlock = start/1024;
	int firstOff = start%1024;
	int firstBlockLocation = current.inode.dBlocks[firstBlock];

	int lastBlock = endChar/1024;
	int lastOff = endChar%1024;
	//int numWrited = 0;
	if(firstBlock != lastBlock){
		for(int i = firstBlock; i<= lastBlock; i++){
			if(i == firstBlock){
				//int numToWrite = 1024 - firstOff;
				int seg = firstBlockLocation/1024;
				int off = firstBlockLocation%1024;
				int dataLocation = this->segPtr*1024 + this->blockPtr;
				current.inode.dBlocks[firstBlock] = dataLocation;		
				//int end = start+howMany;		
				this->writeDataBlock(seg, off, 1024, firstOff, c, iNodeNum, firstBlock);
			}			
			else if(i == lastBlock){
				int lastBlockLocation = current.inode.dBlocks[lastBlock];
				int seg = lastBlockLocation/1024;
				int off = lastBlockLocation%1024;
				int dataLocation = this->segPtr*1024 + this->blockPtr;				
				current.inode.dBlocks[lastBlock] = dataLocation;		
				//int end = start+howMany;		
				this->writeDataBlock(seg, off, lastOff, 0, c, iNodeNum, i);

			}
			else{
				int blockLocation = current.inode.dBlocks[i];
				int seg = blockLocation/1024;
				int off = blockLocation%1024;
				int dataLocation = this->segPtr*1024 + this->blockPtr;
				current.inode.dBlocks[i] = dataLocation;
				this->writeDataBlock(seg, off, 1024, 0, c, iNodeNum, lastBlock);
			}
		}
		int iNodeLoc = this->segPtr*1024 + this->blockPtr;
		iMap[imapPartNum].imap.locations[offset] = iNodeLoc;
	}	
	else if(firstBlock == lastBlock){
		int seg = firstBlockLocation/1024;
		int off = firstBlockLocation%1024;
		int dataLocation = this->segPtr*1024 + this->blockPtr;
		current.inode.dBlocks[firstBlock] = dataLocation;		
		int end = start+howMany;		
		this->writeDataBlock(seg, off, end, firstOff, c, iNodeNum, firstBlock);
		// write inode 
		int iNodeLoc = this->segPtr*1024 + this->blockPtr;
		iMap[imapPartNum].imap.locations[offset] = iNodeLoc;	
	}
	//-----write iNode ----------------------------------------------------
	memcpy(&this->segment[this->blockPtr], &current, 1024);
	// ssb stuuff ---------------------------------------------------------
	int ssbBlock = (this->blockPtr-8)/128;
	int ssbOff = ((this->blockPtr-8)%128)*2;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff] = iNodeNum;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff+1] = -1;
	// end ssb ------------------------------------------------------------
	this->blockPtr++;
	if(this->blockPtr == 1024){	// do seg overflow stuff
		char* fn = (char*)malloc(256);			
		int freeSeg = this->segPtr;
		this->liveness[freeSeg] = 1; //test
		int nextSeg = this->findCleanSeg();	
		if(nextSeg == -1){
			//clean
		}
		this->segPtr = nextSeg;
		string copyStr = "DRIVE/SEGMENT"+to_string(freeSeg);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		int fd = open(fn, O_WRONLY, 0777); // open the file
		write(fd, &this->segment, sizeof(this->segment));
		
		this->blockPtr = 8;
		this->zeroSsb();
		//memcpy(&this->segment[this->blockPtr], &this->inode, 1024);
		close(fd);
	}
	//--------------------------------------------------------------------
	int imapLoc = this->segPtr*1024 + this->blockPtr;
	this->checkpoint[imapPartNum] = imapLoc;
	// update imap
	// write imap---------------------------------------------------------
	memcpy(&this->segment[this->blockPtr], &this->iMap[imapPartNum] , 1024);
	// ssb stuuff ---------------------------------------------------------
	ssbBlock = (this->blockPtr-8)/128;
	ssbOff = ((this->blockPtr-8)%128)*2;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff] = -1;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff+1] = imapPartNum;
	// end ssb ------------------------------------------------------------
	this->blockPtr++;
	if(this->blockPtr == 1024){ // do seg overflow stuff
		char* fn = (char*)malloc(256);			
		int freeSeg = this->segPtr;
		this->liveness[freeSeg] = 1; //test
		int nextSeg = this->findCleanSeg();		
		if(nextSeg == -1){
			//clean
			
		}
		this->segPtr = nextSeg;
		string copyStr = "DRIVE/SEGMENT"+to_string(freeSeg);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		int fd = open(fn, O_WRONLY, 0777); // open the file
		write(fd, &this->segment, sizeof(this->segment));
		this->blockPtr = 8;
		this->zeroSsb();
 
		close(fd);
	}
}



int LFS::writeDataBlock(int segment, int offset, int numChar, int start, char c, int nodeNum, int dBlockNum){
	//cout << "in writeDataBlock" << endl;
	block temp;
	int fd;
	string copyStr;
	
	if(segment == this->segPtr){
		memcpy(&temp, &this->segment[offset], 1024);
	}
	else{
				
		char* fn = (char*)malloc(256);
		copyStr = "DRIVE/SEGMENT"+to_string(segment);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		fd = open(fn, O_RDONLY, 0777);
		lseek (fd, (offset*1024), SEEK_SET); // seek to offset of correct segment //TODO potential bug
		read(fd, &temp, 1024);
		close(fd);
	}
	int ctr = 0;
	//cout << "start " << start << endl;
	//cout << "numChar " << numChar << endl;	
	for(int i = start; i < numChar; i++){
		ctr++;		
		//printf("%c", temp.data.data[i]);
		temp.data.data[i] = c;
		
	}
	//write block to seg
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	bool segOverflow = false;	
	memcpy(&this->segment[this->blockPtr], &temp, 1024);	
	//write in ssb nodeNumber followed by dataCtr
	// ssb stuuff ---------------------------------------------------------
	int ssbBlock = (this->blockPtr-8)/128;
	int ssbOff = ((this->blockPtr-8)%128)*2;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff] = nodeNum;
	this->segment[ssbBlock].ssbBlock.blockInfo[ssbOff+1] = dBlockNum;
	// end ssb ------------------------------------------------------------		
	this->blockPtr++;
	if(this->blockPtr == 1024){
		segOverflow = true;
	}
	if(segOverflow){  
		char* fn = (char*)malloc(256);		
		int freeSeg = this->segPtr;
		this->liveness[freeSeg] = 1;
		int nextSeg = this->findCleanSeg();		
		if(nextSeg == -1){
			//clean
		}
		this->segPtr = nextSeg;
		string copyStr = "DRIVE/SEGMENT"+to_string(freeSeg);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		int fd = open(fn, O_WRONLY, 0777); // open the file
		write(fd, &this->segment, sizeof(this->segment));
		this->blockPtr = 8;
		this->zeroSsb();
	}
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------




	
	return ctr;


	// get data block print the numChar

}

void LFS::zeroSsb(){
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 256; j++){
			this->segment[i].ssbBlock.blockInfo[j] = -2;
		}
	}
}

int LFS::getNumLive(){
	int ctr = 0;	
	for(int i = 0; i < 64; i++){
		if(this->liveness[i]){
			ctr++;
		}
	}
	return ctr;
}

void LFS::clean(){
	cout << "liveness before clean" << endl;	
	this->_seeliveness();	
	this->restart();	
	int numLive = this->getNumLive();	
	//cout << "numLive " << numLive << endl; 
	block** segments = new block*[numLive]; // maybe change to vector 
	for(int i = 0; i < numLive; i++){		// to check size if i end up with
		segments[i] = new block[1024];		// seg faults
	}
	
	// get each seg	
	int fd;
	char* fn = (char*)malloc(256);
	int ctr = 0;	
	for(int i = 0; i < 64; i++){	
		if(this->liveness[i]){	
			string copyStr = "DRIVE/SEGMENT"+to_string(i);
			for (int j = 0; j < int(copyStr.length()); j++) {
				fn[j] = copyStr[j];
			}
			fn[int(copyStr.length())] = '\0';
			fd = open(fn, O_RDONLY, 0777);	
			read(fd, &segments[ctr][0],  1048576);
			ctr++;
			close(fd);
		}
	}
	
	
	//cout << fnm.size() << endl;
	
//	cout << "after getting all live segs" << endl;
	//cout << ctr << endl;

	// get each iNode
	map<string, int>::iterator it;
	int iNodeNum;	
	int imapPart;
	int offset;
	int blockNum;
	int segNum;
	int segOffset;
	string copyStr;
	map<int, block> allNodes;
	for(it = this->fnm.begin(); it != this->fnm.end(); it++){
		//cout << "loop" << endl;		
		iNodeNum = it->second;
		imapPart = iNodeNum/256;
		offset = iNodeNum%256;
		blockNum = iMap[imapPart].imap.locations[offset];
		segNum = blockNum/1024;
		segOffset = blockNum%1024;
		block current;
		copyStr = "DRIVE/SEGMENT"+to_string(segNum);
		for (int j = 0; j < int(copyStr.length()); j++) {
			fn[j] = copyStr[j];
		}
		fn[int(copyStr.length())] = '\0';
		//cout << "after" << endl;
		int fd = open(fn, O_RDONLY, 0777);
		lseek (fd, (segOffset*1024), SEEK_SET); // seek to offset of correct segment //TODO potential bug
		read(fd, &current, 1024); // read in the piece
		//cout << "mark" << endl;
		allNodes.insert(pair<int, block>(iNodeNum, current));
		//cout << "end" << endl;
		
	}
	//cout << "after getting all iNodes" << endl;
	//cout << allNodes.size() << endl;
	//cout << "-----" << endl;	
	// check allNodes 	
	map<int, block>::iterator it2;
	//for(it2 = allNodes.begin(); it2 != allNodes.end(); it2++){
	//	cout << it2->first << " " << it2->second.inode.fileName << endl;
	//}
	int currentSegNum = 0;
	int currBlock = 8;
	//int absoluteBlock = 0;
	block seg[1024];
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 256; j++){
			seg[i].ssbBlock.blockInfo[j] = -2;
		}
	}
	int debugCtr = 0;
	int currSeg = -1;	
	for(int i = 0; i < numLive; i++){ // for each live seg we got
		//cout << i << " " << "loop" << endl;		
		for(int k = 0; k < 64; k++){
			if(liveness[k]){
				currSeg = k;
				break;
			}
		}			
		for(int j = 8; j < 1024; j++){
					
			liveness[currSeg] = 0;
			blockNum = currSeg*1024 + j;
				
			int ssbBlockNum = (j-8)/128;
			int ssbOff = ((j-8)%128)*2;
			int one = segments[i][ssbBlockNum].ssbBlock.blockInfo[ssbOff];
			int two = segments[i][ssbBlockNum].ssbBlock.blockInfo[ssbOff+1];
			//cout << one << "," << two << " ";
			if(one == -2 || one == -1 || two == -1) continue;
			else{
				//cout << debugCtr << endl;
				debugCtr++;				
				int dBlockNodeNum = one;
				int dBlockOffset = two;
				it2 = allNodes.find(dBlockNodeNum);
				if(it2 == allNodes.end()) continue;
				int blockToCompare = it2->second.inode.dBlocks[dBlockOffset];
				if(blockToCompare != blockNum) continue;				
				it2->second.inode.dBlocks[dBlockOffset] = currBlock;
				//memcpy(&seg[currBlock%1024], &it2->second, 1024);
				memcpy(&seg[currBlock%1024], &segments[i][j], 1024);
				// ssb stuuff ---------------------------------------------------------
				int ssbBlock2 = ((currBlock%1024)-8)/128;
				int ssbOff2 = (((currBlock%1024)-8)%128)*2;
				seg[ssbBlock2].ssbBlock.blockInfo[ssbOff2] = dBlockNodeNum;
				seg[ssbBlock2].ssbBlock.blockInfo[ssbOff2+1] = dBlockOffset;
				// end ssb---------n--------------------------------------------------			
				//cout << "currBlock " << currBlock << endl;				
				currBlock++;
				if(currBlock%1024 == 0){ //seg is full, write to mem
					// write seg to mem
					char* fn = (char*)malloc(256);					
					string copyStr = "DRIVE/SEGMENT"+to_string(currentSegNum);
					for (int j = 0; j < int(copyStr.length()); j++) {
						fn[j] = copyStr[j];
					}
					fn[int(copyStr.length())] = '\0';
					int fd = open(fn, O_WRONLY, 0777); // open the file
					write(fd, &seg, sizeof(seg));
					this->liveness[currentSegNum] = 1;
					currentSegNum++;
					currBlock+=8;
					for(int i = 0; i < 8; i++){
						for(int j = 0; j < 256; j++){
							seg[i].ssbBlock.blockInfo[j] = -2;
						}
					}
				}
			}
		}
		//cout << endl;
	}
	map<int, block>::iterator it3;
	// write out all inodes while checking for overflow
	for(it3 = allNodes.begin(); it3 != allNodes.end(); it3++){
		// write out inode
		memcpy(&seg[currBlock%1024], &it3->second, 1024);
		// ssb stuuff ---------------------------------------------------------
		int ssbBlock2 = ((currBlock%1024)-8)/128;
		int ssbOff2 = (((currBlock%1024)-8)%128)*2;
		seg[ssbBlock2].ssbBlock.blockInfo[ssbOff2] = it3->first;
		seg[ssbBlock2].ssbBlock.blockInfo[ssbOff2+1] = -1;
		// end ssb---------n--------------------------------------------------	
		
		//update imap		
		int nodeNum = it3->first;
		int imappiece = nodeNum / 256;
		int off = nodeNum % 256;
		this->iMap[imappiece].imap.locations[off] = currBlock;		
		// 		
		currBlock++;
		if(currBlock%1024 == 0){ //seg is full, write to mem
			// write seg to mem
			char* fn = (char*)malloc(256);					
			string copyStr = "DRIVE/SEGMENT"+to_string(currentSegNum);
			for (int j = 0; j < int(copyStr.length()); j++) {
				fn[j] = copyStr[j];
			}
			fn[int(copyStr.length())] = '\0';
			int fd = open(fn, O_WRONLY, 0777); // open the file
			write(fd, &seg, sizeof(seg)); 
			this->liveness[currentSegNum] = 1;
			currentSegNum++;
			currBlock+=8;
			for(int i = 0; i < 8; i++){
				for(int j = 0; j < 256; j++){
					seg[i].ssbBlock.blockInfo[j] = -2;
				}
			}
		}
	}
	// write out all imap pieces while checking for overflow
	for(int i = 0; i < 40; i++){
		memcpy(&seg[currBlock%1024], &this->iMap[i], 1024);
		// ssb stuuff ---------------------------------------------------------
		int ssbBlock2 = ((currBlock%1024)-8)/128;
		int ssbOff2 = (((currBlock%1024)-8)%128)*2;
		seg[ssbBlock2].ssbBlock.blockInfo[ssbOff2] = -1;
		seg[ssbBlock2].ssbBlock.blockInfo[ssbOff2+1] = i;
		// end ssb---------n--------------------------------------------------			
	
	
		this->checkpoint[i] = currBlock;
		currBlock++;
		if(currBlock%1024 == 0){ //seg is full, write to mem
			// write seg to mem
			char* fn = (char*)malloc(256);					
			string copyStr = "DRIVE/SEGMENT"+to_string(currentSegNum);
			for (int j = 0; j < int(copyStr.length()); j++) {
				fn[j] = copyStr[j];
			}
			fn[int(copyStr.length())] = '\0';
			int fd = open(fn, O_WRONLY, 0777); // open the file
			write(fd, &seg, sizeof(seg)); 
			this->liveness[currentSegNum] = 1;
			currentSegNum++;
			currBlock+=8;
			for(int i = 0; i < 8; i++){
				for(int j = 0; j < 256; j++){
					seg[i].ssbBlock.blockInfo[j] = -2;
				}
			}
		}
	}	

	
	// write last partial seg
	// write seg to mem
	
	fn = (char*)malloc(256);					
	copyStr = "DRIVE/SEGMENT"+to_string(currentSegNum);
	for (int j = 0; j < int(copyStr.length()); j++) {
		fn[j] = copyStr[j];
	}
	fn[int(copyStr.length())] = '\0';
	fd = open(fn, O_WRONLY, 0777); // open the file
	write(fd, &seg, sizeof(seg));
	this->liveness[currentSegNum] = 1;
	cout << "liveness after clean" << endl;	
	this->_seeliveness();
	
	
}

//-------------------------debug func------------------------------

void LFS::_seessb(){
	cout << "--------------------seessb---------------------" << endl;	
	int stopVal = 0;	
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 256; j+=2){
			cout << this->segment[i].ssbBlock.blockInfo[j] << ",";
			cout << this->segment[i].ssbBlock.blockInfo[j+1] << " ";
		}
		cout << endl;
		if(i == stopVal) break;
	}
	cout << endl;
	cout << "----------------------------------------------------" << endl;
}

void LFS::_seeimap(){
	int stopVal = 1;	// change this to see more or less of imap
	cout << "----------seeImap----------" << endl;    
	for(int i = 0; i < 40; i++){
		cout << "Piece " << i << endl;
    	for(int j = 0; j < 256; j++){	
			cout << this->iMap[i].imap.locations[j] << " ";
		}
		cout << endl;
		if(i == stopVal) break;
	}
	cout << "--------------------------" << endl;
}


void LFS::_seeliveness(){
	cout << "--------------liveness-------------" << endl;	
	for(int i = 0; i < 64; i++){
		printf("%d", this->liveness[i]);
		cout << " ";
	}
	cout << endl;
	cout << "-----------------------------------" << endl;
}


