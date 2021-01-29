#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

//a linked list of linked list will be used store the files and there word distributions
//The FileList struct is used to make a linked list of all the files. It has a name, a next FileList, and it also points to a WordList struct.
//The WordList struct stores the data needed for each word in the file. token stores a pointer to the word itself, count stores at first the number
//of times that the word appears but eventually will be changed to the count of the word/total_words in the file. Each WordList also points to a next WordList for the next unique word in the file

struct WordList{
	char *token; // the word that is stored
	double count; // initially tracks number of occurences of the word but eventually stores num occurences / total number of tokens (probability, needed for calculations)
	struct WordList *next;
};
struct FileList{
	char *file_name; 
	struct FileList *next;
	struct WordList *first; //pointer to first node of the wordlist struct that contains the token distribution for the file
    int numTokens; // total number of tokens in the file
};


// a kldList struct is used to store the resulting Jensen-Shannone Distance every time we calculate it for two files. the struct is used to make a linked list of all of the calculations which we then are able to sort by the combined number of tokens for the two files so that we can print the output in the correct order.
struct kldList{
	double KLD; // Jensen Shannon Distaince
	int count; // combined number of tokens in files with names stored in file1 and file2 respectively
	struct kldList *next;
	char* file1;
	char* file2;
};

//args for threaded fildDirectories function
struct DirArgs {
    char* path;		//current directory path
    DIR* directory;	//pointer to opened directory
    struct FileList* files;	//pointer to head of shared FileList
    pthread_mutex_t* lock;	//pointer to mutex lock
};

//args for threaded addFile function
struct FileArgs {
    char* path;			//file path
    struct FileList* files;	//pointer to head of shared FileList
    pthread_mutex_t* lock;	//pointer to mutex lock
};

//Prints WordLists out
void print_WordList(struct WordList* wordlist){
	while(wordlist != NULL){
		printf("Token: %s Count: %lf\n", wordlist->token, wordlist->count);
		wordlist = wordlist->next;
	}
    printf("\n\n");
}
//Based on the Jensen-Shannon Distance that this function is passed, it prints out the result in different colors as specified in the assignment
void print_result(double kld, char* file1, char* file2){
	if(kld <= 0.1 ){
		printf("\x1B[31m%lf\x1B[0m \"%s\" and \"%s\"\n", kld, file1, file2);
	}else if(kld<=0.15){
		printf("\x1B[33m%lf\x1B[0m \"%s\" and \"%s\"\n", kld, file1, file2);
	}else if(kld<=0.2){
		printf("\x1B[32m%lf\x1B[0m \"%s\" and \"%s\"\n", kld, file1, file2);
	}else if(kld<=0.25){
		printf("\x1B[36m%lf\x1B[0m \"%s\" and \"%s\"\n", kld, file1, file2);
	}else if(kld<=0.3){
		printf("\x1B[34m%lf\x1B[0m \"%s\" and \"%s\"\n", kld, file1, file2);
	}else{
		printf("\x1B[37m%lf\x1B[0m \"%s\" and \"%s\"\n", kld, file1, file2);
	}
}

//Calculates the Kullbeck-Liebler Divergence given a word distribution for a file stored in list1 and a mean construction stored in mean
//returns Kullbeck-liebler Divergence
double KLD(struct WordList* list1, struct WordList* mean){
	struct WordList* temp = list1; //iterator variable
	double ret = 0;
	while(temp!=NULL){ //iterate through file's WordList
		struct WordList* temp2 = mean;
		while(temp2!=NULL){ // iterate through mean constr WordList
			if(strcmp(temp->token, temp2->token)==0){ //if both are the same word, we calculate the formula below and add that to the total KLD
				ret += (temp->count)*log10((temp->count)/(temp2->count));
				break;
			}else{ // if not, continue
				temp2 = temp2->next;
			}
		}
		temp = temp->next;
	}
	return ret;

}

//Given two files word distributions, this creates a mean construction for them and then calls KLD to calculate Kullbeck-Liebler Divergence for each and then returns Jensen-Shannon Distance calculated from those
double mean_dist(struct WordList* wordlist_1, struct WordList* wordlist_2){
	struct WordList* ret = NULL; //will store mean construction
	struct WordList* temp = wordlist_1;
	struct WordList* temp2 = wordlist_2;
	struct WordList* temp3 = NULL; // keeps track of mean constructions first node
	while(temp!= NULL){// iterate through first file's wordlist
		int same = 0;
		if(ret == NULL){ //if mean construction is empty, create it and set it's token to first word in first file
			ret = (struct WordList*)malloc(sizeof(struct WordList));
			ret->token = temp->token;
			ret->next = NULL;
			temp3 = ret;
		} else{ //otherwise do the same for ret->next
			ret->next = (struct WordList*)malloc(sizeof(struct WordList));
			ret = ret->next;
			ret->token = temp->token;
			ret->next = NULL;

		}
		while(temp2!=NULL){ //iterate through second wordlist. If we find that the word is present in both add their distribution and divide it by 2, else just divide by 2
			if(strcmp(temp2->token, temp->token) == 0){
				same = 1;
				break;
			} else{
				temp2 = temp2->next;
			}
		}
		if(same == 1){
			ret->count = (temp->count + temp2->count)/2;
		}else{
			ret->count = temp->count/2;
		}
		temp2 = wordlist_2; //reset temp2 to the first node it wordlist_2
		temp = temp->next; 
	}
	//only added words that are present in both files OR only present in file 1, need to add unqiue file 2 words
	temp2 = wordlist_2;
	temp = wordlist_1;
	while(temp2!=NULL){
		if(temp == NULL){ // there were no tokens in the file whose wordList is wordlist_1. therefore the mean construction has not been initialized yet
			if(ret == NULL){ //if mean construction is empty, create it and set it's token to first word in first file
			ret = (struct WordList*)malloc(sizeof(struct WordList));
			ret->token = temp2->token;
			ret->count = temp2->count/2; // there are no tokens in wordlist_1 so immediately divide by 2
			ret->next = NULL;
			temp3 = ret;
		} else{ //otherwise do the same for ret->next
			ret->next = (struct WordList*)malloc(sizeof(struct WordList));
			ret = ret->next;
			ret->count = temp2->count/2;
			ret->token = temp2->token;
			ret->next = NULL;

		}
		}else{  // wordlist_1 was not empty
		ret = temp3; // set ret to head of mean construction
		while(ret!=NULL){ // all nodes left only present in wordlist_2, skip nodes already in ret and divide count by 2 when adding to ret
			if(strcmp(ret->token,temp2->token)==0){
				break;
			}else if(ret->next == NULL){ 
				ret->next = (struct WordList*)malloc(sizeof(struct WordList));
				ret = ret->next;
				ret->token = temp2->token;
				ret->count = temp2->count/2;
				ret->next = NULL;
			}else{
				ret = ret->next;
			}
		}
		}
		temp2 = temp2->next;
	}	
	ret = temp3;	// temp3 stored first node of mean construction
	double kld = KLD(wordlist_1, ret);
	kld += KLD(wordlist_2, ret);
	kld = kld/2; //Find KLD for each file, add them, and divide by 2 to get Jensen-Shannon Distance
	while(ret != NULL){ // freeing
		temp3 = ret;
		ret = ret->next;
		free(temp3);
	}
	return kld;


}


// Given a filename, this function creates a FileList object and WordList object for said file and populates that WordList for that file. Returns pointer to the created FileList
struct FileList* word_dist(char* filename){
	const char* delims = " \n";
	struct FileList *file = (struct FileList*) malloc(sizeof(struct FileList));
	struct WordList *word = (struct WordList*) malloc(sizeof(struct WordList));
	word->count = 0;  // used to check if wordlist is empty
	word->token = NULL;
	file->file_name = filename;
	file->next = NULL;
	file->first = word;
	file->numTokens = 0;
	int total_count = 0;  //stores count of number of tokens in file
	FILE *fp;
	fp = fopen(filename, "r");
	if(fp == NULL){
		free(word);
		file->first = NULL;
		return file;
	}
	//find size of file
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(size ==0){
		free(word);
		file->first = NULL;
		return file;
	}
	char* file_string = malloc(sizeof(char)*size);
	while(fgets(file_string, size, fp)!= NULL){  //iterate through file line by line
	
		char* the_token2 = strtok(file_string, delims);
		while(the_token2 != NULL){
			//loop through all the tokens. if a token is a repeat word we will add one to the count for that word. otherwise we will make a new node and add that to the list
			int length = strlen(the_token2);
			char* token_copy = (char*)malloc(sizeof(char)*(length + 1)); //used to store what token will be after removing punctuation, etc.
			char* the_token = (char*)malloc(sizeof(char)*(length+1));
			strcpy(the_token, the_token2);
			int i, pos=0;
			for(i = 0; i < length; i++){ //processing node to remove non alphabetic characters(for scope of project, generally only punctuation)
				if(isalpha(the_token[i]) || the_token[i] == '-'){ //only add to new string if alphabetic OR hyphens
					if(isalpha(the_token[i])){
						token_copy[pos] = tolower(the_token[i]);
					}else{
						token_copy[pos] = the_token[i];
					}
					pos++;
				}		
			}
			token_copy[pos] = '\0';
			// copy token with all the non alphabetic character removed back into the_token
			strncpy(the_token,token_copy, strlen(token_copy));
			free(token_copy);
			the_token[pos] = '\0';	
			if(strcmp(the_token, "")!=0){// means that token was entirely non alphabetic
				if(file->first->count == 0){ // count is always initialized to 0 so this only is true if we haven't added any tokens yet
					file->first->token = the_token;
					file->first->next = NULL;
					file->first->count = 1;
				total_count++;
				}else{
					struct WordList *curr = file->first;
					while(curr != NULL){
						if(strcmp(curr->token, the_token)==0){// if token is already in wordlist, increase count, otherwise create new node for new token
							curr->count +=1;
							total_count++;
						free(the_token);
						break;
						}else if(curr->next == NULL){
							struct WordList *temp = (struct WordList*)malloc(sizeof(struct WordList));
							temp->token = the_token;
							temp->count = 1;
							temp->next = NULL;
							curr->next = temp;
							total_count++;
						break;
						}else{
							curr = curr->next;
						}
					}
			
				}
			}else{
				free(the_token);
			}

			the_token2 = strtok(NULL, delims);
		}
	}
	file->numTokens = total_count; //store number of tokens in the file for use in calculations later
	//struct WordList *temp = file->first;
	//temp->count stored number of occurences for each word but we need probability for calculations so divide the count for each word by the total count
	if(file->first->count ==0){
		free(word);
		file->first = NULL;
	}
	struct WordList *temp = file->first;
	while(temp != NULL){

		//printf("token %s", temp->token);
		temp->count = (temp->count)/total_count;
		temp = temp->next;
	}
	free(file_string);
	fclose(fp);
	return file;

}

//function to add file to shared FileList
//uses word_dist to tokenize file
//implements mutex lock to ensure shared FileLock cannot be accessed by two threads at the same time
void* addFile (void* input) {
    struct FileArgs* args = (struct FileArgs*) input;
	struct FileList* new_list = word_dist(args->path);
    pthread_mutex_lock(args->lock);				//all code for accessing shared FileList is within the locked region
    if (args->files->file_name == NULL) {
        args->files->file_name = new_list->file_name;
        args->files->next = NULL;
        args->files->first = new_list->first;
        args->files->numTokens = new_list->numTokens;
    } else{
        struct FileList* ptr = args->files;
        while (ptr->next != NULL) {
		    ptr = ptr->next;
	    }
        ptr->next = (struct FileList*) malloc(sizeof(struct FileList));
        ptr = ptr->next;
        ptr->file_name = new_list->file_name;
        ptr->next = NULL;
        ptr->first = new_list->first;
        ptr->numTokens = new_list->numTokens;
    }
    pthread_mutex_unlock(args->lock);
    free(new_list);




    return NULL;
}

//function to recursively iterate through directory contents
//finding a subdirectory creates new thread to handle it
//finding a file creates new thread to tokenize it
void* findDirectories (void* input) {
    struct DirArgs* args = (struct DirArgs*) input;
    struct dirent* ptr;
    while ((ptr = readdir(args->directory))) {
        if (ptr->d_type == DT_DIR) {                          //new directory found, open and recursively call function
            if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) continue;
            char *path = (char*)malloc(sizeof(char) * (strlen(args->path) + strlen(ptr->d_name) + 2));
            path[0] = '\0';
            strcat(path, args->path);
            strcat(path, ptr->d_name);
            strcat(path, "/");
            struct DirArgs new_args;
            new_args.path = path;
            new_args.directory = opendir(path);
            new_args.files = args->files;
            new_args.lock = args->lock;
            pthread_t thread;
            pthread_create(&thread, NULL, findDirectories, &new_args);
            pthread_join(thread, NULL);
            free(path);
            closedir(new_args.directory);
        } else if (ptr->d_type == DT_REG) {                 //new file to be tokenized and added to FileList
            if (strcmp(ptr->d_name, "detector") == 0) continue; 
            char *path = (char*)malloc(sizeof(char) * (strlen(args->path) + strlen(ptr->d_name) + 1));
            path[0] = '\0';
            strcat(path, args->path);
            strcat(path, ptr->d_name);
            struct FileArgs file_args;
            file_args.path = path;
            file_args.files = args->files;
            file_args.lock = args->lock;
            pthread_t thread;
            pthread_create(&thread, NULL, addFile, &file_args);
            pthread_join(thread, NULL);
        }
    }
    return NULL;
}
//creates ordered kldList struct(order by most combined tokens to least) and then calls print_result() for each kldList
//The function should only be passed the FileList once it is completely populated 
void order_print(struct FileList* start){
	int i = 0;
	int breakvar = 0;
	struct kldList* kldList_ptr = NULL;
	while(1){  //finds all combinations of files in FileList, breakvar used to check when all are exhausted
		struct FileList* curr = start;
		int j;
		for(j=0; j< i; j++){
			curr = curr->next;
			if(curr == NULL){
				breakvar = 1; //all possibilities exhausted, break out of loop.
			}
		}
		if(breakvar == 1){
			break;
		}
		struct FileList* curr2 = curr->next;
		while(curr2 != NULL){ //iterate through rest of list, adding all new combinations with node curr
			if(kldList_ptr == NULL){ // if kld list is empty, create first node
				kldList_ptr = (struct kldList*)malloc(sizeof(struct kldList));
				kldList_ptr->next = NULL;
				kldList_ptr->count = curr->numTokens + curr2->numTokens;
				kldList_ptr->KLD = mean_dist(curr->first, curr2->first);
				kldList_ptr->file1= curr->file_name;
				kldList_ptr->file2 = curr2->file_name;
				curr2 = curr2->next;
			} else{ // if not empty, create new node and determine it place in the list based on kldList->count (number of tokens in both files combined)
				struct kldList* kldNew = (struct kldList*) malloc(sizeof(struct kldList));
				kldNew->count = curr->numTokens+curr2->numTokens;
				kldNew->KLD = mean_dist(curr->first, curr2->first);
				kldNew->file1 = curr->file_name;
				kldNew->file2 = curr2->file_name;
				struct kldList* kldCurr = kldList_ptr;
				if(kldNew->count > kldCurr->count){ //new node has largest num tokens
					kldNew->next = kldCurr;
					kldList_ptr = kldNew;
				}else{
					struct kldList* kldPrev;
					while(kldCurr != NULL){
						if(kldCurr->count >= kldNew->count){
							if(kldCurr->next ==NULL){ //new node has smallest num tokens
								kldCurr->next = kldNew;
								kldNew->next = NULL;
								break;
							}else{
								kldPrev = kldCurr;
					       			kldCurr = kldCurr->next;
							}
						} else{ // found the position for new node if not smallest or largest
							kldPrev->next = kldNew;
							kldNew->next = kldCurr;
							break;
						}
					}
			
				}
				curr2=curr2->next;		
			
			}

		}
		
		i++; // increases where in the list we start making combinations from
	}
	if(kldList_ptr == NULL){ //catches if there was only 1 useable file
		printf("Only one useable file was found in the directory\n");
		return;
	}

	while(kldList_ptr != NULL){ //kldList_ptr is now sorted by number of tokens in the files of each node, so we print the answer
		print_result(kldList_ptr->KLD, kldList_ptr->file1, kldList_ptr->file2);
		struct kldList* freer = kldList_ptr;
		kldList_ptr = kldList_ptr->next;
		free(freer);
		
	}
}
int main (int argc, char* argv[]) { 

    if (argc < 2 ) {
        printf("Error: Specify a starting directory\n");
        return 0;
    } else if (argc > 2) {
        printf("Error: Too many arguments\n");
        return 0;
    }else if (strcmp(argv[1], "")==0) {
        printf("Error: Specify a starting directory\n");
        return 0;
    }else if(strcmp(argv[1],"/" )==0){
	printf("Error: invalid directory\n");
	return 0;	
    }

    DIR* start_directory = NULL;
    int toFree = 0;
    char* start=NULL;
    if (argv[1][strlen(argv[1]) - 1] == '/') {
        start = argv[1];
    } else {
	start = (char*)malloc(sizeof(char) * (strlen(argv[1]) + 2));
	start[0] = '\0';
	strcat(start, argv[1]);
	strcat(start, "/");
	toFree = 1;
    }
    start_directory = opendir(start);
	
   if (start_directory == NULL) {
	if(toFree == 1){
	   free(start);
	}
        printf("Error : Directory could not be found\n");
        return 0;
    }

    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    struct DirArgs args;
    args.path = start;
    args.directory = start_directory;
    args.files = (struct FileList*) malloc(sizeof(struct FileList));
    args.files->file_name = NULL;
    args.lock = &lock;



    


    findDirectories(&args);
	
	if (args.files->file_name == NULL) {
		printf("Error: directory is empty\n");
		free(args.files);
	} else {
    	order_print(args.files);
		struct FileList* freer = args.files;
		struct WordList* freer2 = args.files->first; 
		while(args.files != NULL){ //freeing malloced variables
			args.files = args.files->next;
			while(freer2 != NULL){
				struct WordList* temp = freer2;
				freer2 = freer2->next;
				free(temp->token);
				free(temp);
			}
			free(freer->file_name);
			free(freer);
			freer = args.files;
			if(args.files!=NULL){
				freer2 = args.files->first;
			}
    	}
	}
	
    if (toFree == 1) free(start);

    closedir(args.directory);


    return 0;
}
