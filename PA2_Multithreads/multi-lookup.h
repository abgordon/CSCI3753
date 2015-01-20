#ifndef MULTILOOKUP_H
#define MULTILOOKUP_H


#include "util.h" 



#define MINARGS 3
#define USAGE "<inputFilePath1> <inputFilePath2> ... <inputFilePathN> <outputFilePath>"
#define SBUFSIZE 1025 //max_name_length
#define INPUTFS "%1024s"

//from assignment writeup
#define MIN_RESOLVER_THREADS 2
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

//new functions for thread routines
void* request_IP(void* file_descriptor);
void* resolve_IP_Name(void* file_descriptor);


#endif