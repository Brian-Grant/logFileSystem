#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bits/stdc++.h> 
#include <map>
#include <fstream>

using namespace std;

class HardDrive {
	public:
		HardDrive();
	private:
		void writeFiles();
};
