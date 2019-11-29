#include "HardDrive.h"

using namespace std;


typedef struct iNode {
	char fileName[128];
	int size;
	int dBlocks[128];
	char dummy[380];
} iNode;

typedef struct dataBlock {
	char data[1024];
} dataBlock;

typedef struct iMapPart {
	int locations[256];
} iMapPart;

typedef struct ssb {
	int blockInfo[256];
} ssb;

typedef union block {
	iNode inode;
	dataBlock data;
	iMapPart imap; 
	ssb ssbBlock;
} block;

class LFS {
	public:
		iNode inode;
		block segment[1024];
		block iMap[40];
		int checkpoint[40];
		char liveness[64];

		map<string, int> fnm;

	
		int segPtr; // pointing to seg number
		int blockPtr; //pointing to block in seg 
		
		
		LFS();
		~LFS();
	

		
		void import(string filename, string lfs_filename);
		void writeToDisk();

		void shutDown();
		void restart();
		int findFreeInode();

		void loadFNM();
		void loadCR();
		void loadIMap();
		void initIMap();
	
		int findCleanSeg();

		void listInfo();
		void writeFNM();
		void writeCR();

		void remove(string lfs_filename);

		void cat(string lfs_filename);
		int printDataBlock(int segment, int offset, int numChar, int start);
		void display(string lfs_filename, int howMany, int start);

		void overwrite(string lfs_filename, int howMany, int start, char c);
		int writeDataBlock(int segment, int offset, int numChar, int start, char c, int nodeNum, int dBlockNum);
		void zeroSsb();
		void clean();
		int getNumLive();

		// debug
		void _seeimap();
		void _seeliveness();
		void _seessb();
	private:


};
