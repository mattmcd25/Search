#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
using namespace std;

#define READ_MODE 1
#define MMAP_MODE 2

#define DEBUG 0

int mode; // read or mmap
int param; // extra param -> chunk size in read mode, thread count in mmap mode
char* term; // search term
char* buf; // buffer

int total; // total file size
int offset; // offset between threads for multithreaded mode

int found = 0; // number of times the search string has been found
int count = 0; // count of how many correct characters have been found so far

sem_t mutex;

/* The search algorithm. Follows the basic solution, IE, bababy will not be detected */
int searchFile(int startVal, int topVal, int* counter) {
	int found = 0;

		for(int i = startVal; i < topVal; i++) {
			if(buf[i] == term[(*counter)]) {
				(*counter) ++;
				if(*counter == strlen(term)) {
					found ++;
					(*counter) = 0;
				}
			} else (*counter) = 0;
			if(mode == READ_MODE) buf[i] = 0;

			//extra logic for peeking into next chunk
			if((i == topVal-1) && (mode == MMAP_MODE) && (param > 1)) { //last loop during a thread's execution
				if((topVal < total) && ((*counter) > 0)) { //theres more to check and we have the start of a term
					for(int j = (i+1); j < total; j++) {
						if(buf[j] == term[(*counter)]) {
							(*counter) ++;
							if(*counter == strlen(term)) { //quit immediately after finding the end of the term
								found++;
								(*counter) = 0;
								return found;
							}
						} else { //give up immediately if the pattern doesnt continue
							(*counter) = 0;
							return found;
						}
					}
				}
			}
		}

		return found;
}

/* Convenience function for the searchFile calls that just use the globals */
int searchFile(int topVal) {
	return searchFile(0, topVal, &count);
}

/* Function that handles the individual threads running */
void *runThread(void* val) {
	int* num = (int*) val;

	int mycount = 0;
	//ex. if the file is 13 bytes and they want 3 threads, it would default to
	//being 0-3, 4-7, 8-12 and 13 would be cut off. this code makes the last thread
	//take that remainder, so it becomes 4+4+5=13.
	int myfinish = ((*num)/offset == (param-1)) ? (total) : *num+offset;

	//search using locals instead of globals for obvious reasons
	int myfound = searchFile(*num, myfinish, &mycount);

	if(DEBUG) cout << "i started at " << *num << ", i found " << myfound << ", and i ended at " << mycount << endl;

	//ensure mutual exclusion, then add results to the global
	sem_wait(&mutex);
	found += myfound;
	sem_post(&mutex);
}

int main(int argc, char* argv[]) {
	if((argc < 3) || (argc > 4)) { // wrong number of args
		cout << "Syntax error- correct syntax is ./proj4 srcfile searchstring [size|mmap]" << endl;
		return 1;
	}
	else if(argc == 3) { // only 3 args
		mode = READ_MODE;
		param = 1024;
		if(DEBUG) cout << "read 1024" << endl;
	}
	else if(argc == 4) {
		if(strcmp(argv[3], "mmap") == 0) { // mmap
			mode = MMAP_MODE;
			if(DEBUG) cout << "mmap" << endl;
		}
		else if(argv[3][0] == 'p') { // threads
			param = atoi((argv[3])+1);
			if(param <= 0 || param > 16) {
				cout << "Error- thread count for mmap() must be between 1 and 16" << endl;
				return 1;
			}
			mode = MMAP_MODE;
			if(DEBUG) cout << "mmap " << param << endl;
		}
		else if((atoi(argv[3]) <= 0) || (atoi(argv[3]) > 8192)) { // invalid chunk size
			cout << "Error- chunk size for read() must be between 1 and 8192" << endl;
			return 1;
		}
		else { // chunk size
			mode = READ_MODE;
			param = atoi(argv[3]);
			if(DEBUG) cout << "read " << param << endl;
		}
	}

	term = argv[2];

	int file = open(argv[1], O_RDONLY);
	if (file == -1) {
		cout << "Unable to open file " << argv[1] << "." << endl;
		return 1;
	}

	int got = 0; // bytes returned by read
	total = 0; // total bytes

	if(mode == READ_MODE) {
		buf = (char*)calloc(sizeof(char), param);

		// simply just search
		while((got = read(file, buf, param)) != 0) {
			total += got;

			found += searchFile(param);
		}
	}
	else if(mode == MMAP_MODE) {
		struct stat sb;

		// configure mmap stuff
		if(fstat(file, &sb) < 0){
			cout << "Could not stat file to obtain its size" << endl;
			return 1;
		}

		buf = (char*) mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, file, 0);

	    if (buf == (char *)-1)	{
	    	cout << "Could not mmap file";
	    	return 1;
	    }

	    total = sb.st_size;

	    if(param > total) {
	    	param = total; // last thread isnt needed if there are more threads than bytes!
	    }

	    // multithreaded mode
	    if(param > 1) {
			vector<pthread_t> threads;
			int* starts = (int*)calloc(sizeof(int), param);
			offset = total/param;

			sem_init(&mutex, 0, 1);

			// make and run all the threads
			for(int i = 0; i < param; i++) {
				starts[i] = i*offset;
				void *runThread(void*);
				pthread_t id;
				threads.push_back(id);
				if(DEBUG) cout << i << endl;
				if(pthread_create(&threads[i], NULL, runThread, (void*)&starts[i])) {
					perror("pthread_create");
					return 1;
				}
			}

			// join all the threads
			for(int i = 0; i < threads.size(); i++) {
				pthread_join(threads[i], NULL);
			}

		    sem_destroy(&mutex);
	    }
	    else {
	    	// normal mmap mode
	    	found += searchFile(total);
	    }

	    if(munmap(buf, sb.st_size) < 0){
	    	cout << "Could not unmap memory" << endl;
			return 1;
	    }

	}

	cout << "File size: " << total << " bytes." << endl;
	cout << "Occurrences of the string \"" << term << "\": " << found << endl;

	close(file);

	return 0;
}
