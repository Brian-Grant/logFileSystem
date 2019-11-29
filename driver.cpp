#include "LFS.h"


string tester(int ctr);

int main(int argc, char **argv) {
	HardDrive *hd = new HardDrive();
	LFS *lfs = new LFS();
	//block b;	
	//cout << "size" << endl;	
	//cout << sizeof(b) << endl;
	//cout << "$ ";
	string input;	
	string first;
	
	/*getline(cin, input);
	stringstream inputStream(input);
	inputStream >> first;*/

	string fileName;
	string lfs_fileName;
	int testCtr = 0;
	//cout << first << endl;	
	while(first != "Shutdown"){
		cout << "$ ";
		getline(cin, input);	     // just uncomment/comment these two lines if you 
		//input = tester(testCtr);   // want to enter commands by hand
		testCtr++;	
		stringstream inputStream(input);
		inputStream >> first;

		if (first == "Shutdown") {
			lfs->shutDown();
			break;
		}
		// this is here for testing purposes, can be commented out when done		
		else if(first == "clean"){
			lfs->clean();
		}		
		
		else if(first == "import"){
			inputStream >> fileName;
			inputStream >> lfs_fileName;
			lfs->import(fileName, lfs_fileName);		
		}
		else if(first == "remove"){
			inputStream >> lfs_fileName;
			lfs->remove(lfs_fileName);
			
		}
		else if(first == "list"){
			lfs->listInfo();
		
		} 
		else if(first =="restart"){
			lfs->restart();
		}
		else if(first == "cat"){
			inputStream >> lfs_fileName;
			lfs->cat(lfs_fileName);
		}
		else if(first == "display"){
			int howMany;
			int start;			
			inputStream >> lfs_fileName;
			inputStream >> howMany;
			inputStream >> start;
			lfs->display(lfs_fileName, howMany, start);
		}		
		else if(first == "overwrite"){
			int howMany;
			int start;
			char c;
			inputStream >> lfs_fileName;
			inputStream >> howMany;
			inputStream >> start;
			inputStream >> c;
			lfs->overwrite(lfs_fileName, howMany, start, c);
		}
		else {
			cout << "invalid command" << endl;
		}

		inputStream.str("");
		/*cout << "$ ";
		getline(cin, input);
		inputStream.str("");		
		stringstream inputStream(input);
		inputStream >> first;*/
		//cout << first << endl;
	}
	// if we are here perform shutdown
	delete(hd);
	delete(lfs);
	return 0;
}


string tester(int ctr){
	string out[39] = { "import fun.txt test1",
					"import test2.txt test2",
					"import fun.txt test3",
					"import test2.txt test4",
					"import fun.txt test5",
					"import test2.txt test6",
					"import fun.txt test7",
					"import test2.txt test8",
					"import fun.txt test9",
					"import fun.txt test10",
					"import test2.txt test11",
					"import fun.txt test12",
					"import test2.txt test13",
					"import fun.txt test14",
					"import test2.txt test15",
					"import fun.txt test16",
					"import test2.txt test17",
					"import fun.txt test18",
					"import fun.txt test19",
					"import test2.txt test20",
					"import fun.txt test21",
					"import test2.txt test22",
					"import fun.txt test23",
					"import test2.txt test24",
					"import fun.txt test25",
					"import test2.txt test26",
					"import fun.txt test27",
					"import fun.txt test28",
					"import test2.txt test29",
					"import fun.txt test30",
					"import test2.txt test31",
					"import fun.txt test32",
					"import test2.txt test33",
					"import fun.txt test34",
					"import test2.txt test35",
					"import fun.txt test36",
					"list",
					"remove test1",
					"Shutdown" };
	return out[ctr];

}
