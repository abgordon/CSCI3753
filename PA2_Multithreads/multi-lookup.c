/*
 * File: multi-lookup.c
 * Author: Andrew Gordon
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2012/02/01
 * Modify Date: 2014/10/14
 * Description:
 * This is the modified lookup.c that contains the multi-threaded solution to the assignment
 *  
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h> 

#include "multi-lookup.h"
#include "queue.h"
#include "util.h"

#define QUEUESIZE 10

//globals
int open_requests = 0; 
pthread_mutex_t queue_mutex; 	//the shared memory where we have a queue of requests
pthread_mutex_t write_mutex; 	//the shared memory where we write our results (output file)
pthread_mutex_t counter_mutex;	//the shared memory where we keep track of count
pthread_cond_t queueNotFull = PTHREAD_COND_INITIALIZER; 
queue q; //queue for producer/consumers

int main(int argc, char* argv[]){

    /* Check Arguments */
    if(argc < MINARGS){
	fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
	fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
	return EXIT_FAILURE;
    }


    /* Local Vars */
    FILE* outputfp = NULL;
    int i;
    int rc; //return code for thread production
    int numCPU = sysconf(_SC_NPROCESSORS_ONLN); //get number of cores for a linux

    int num_requesters = argc-2; //(1+num args)-2
    pthread_t requesters[num_requesters];open_requests = num_requesters; //available requests same as num requesters at first

    int num_resolvers = numCPU >= MIN_RESOLVER_THREADS ? numCPU : MIN_RESOLVER_THREADS;
    pthread_t resolvers[num_resolvers];


    

    /* Open Output File */
    outputfp = fopen(argv[(argc-1)], "w");
    if(!outputfp){
	perror("Error Opening Output File");
	return EXIT_FAILURE;
    }

   	//queue_init
    if (queue_init(&q,QUEUESIZE) == QUEUE_FAILURE)
    {
    	fprintf(stderr, "queue_init() failed!");
    	return EXIT_FAILURE;
    }
    //mutex init
    if (pthread_mutex_init(&queue_mutex, NULL) ||
    	pthread_mutex_init(&write_mutex, NULL) ||
    	pthread_mutex_init(&counter_mutex, NULL))
		{
			fprintf(stderr, "A mutex init failed!");
			return EXIT_FAILURE;
		}


	//thread spawn: requesters
	//pthread create params: pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	//										void *(*start_routine) (void *), void *arg)
	//create request threads to run routine "request_IP" with our command line args
	//keep return code to errcheck
    for(i=0;i<num_requesters;i++){
	rc = pthread_create(&(requesters[i]), NULL, request_IP, &(argv[i+1]));
	if (rc){
	    printf("ERROR; return code from pthread_create() is %d\n", rc);
	    exit(EXIT_FAILURE);
		}
    }


    //thread spawn: resolvers
    //same as above, but call routine resolve_ip with file descriptor of output
    for (i=0;i<num_resolvers; i++)
    {
    rc = pthread_create(&(resolvers[i]), NULL, resolve_IP_Name, &(outputfp));
	if (rc){
	    printf("ERROR; return code from pthread_create() is %d\n", rc);
	    exit(EXIT_FAILURE);
		}
    }

    //pthread join args: (pthread_t thread, void **value_ptr)
    //wait for everyone to finish: suspend execution of the calling thread until target thread terminates
    for (i = 0; i < num_requesters; i++)
    {
    	pthread_join(requesters[i], NULL);
    }
    for (i = 0; i < num_resolvers; i++)
    {
    	pthread_join(resolvers[i], NULL);
    }




    /* Close Output File */
    fclose(outputfp);
    //remove our mutexes
    queue_cleanup(&q);
    if(pthread_mutex_destroy(&queue_mutex) ||
		pthread_mutex_destroy(&counter_mutex) ||
		pthread_mutex_destroy(&write_mutex))
    {
    	fprintf(stderr, "failed to destroy mutex!\n");
    }

    return EXIT_SUCCESS;
}


/* code for requesters */
void* request_IP(void* file_descriptor) 
{
	FILE* inputfp;
	char** fname = file_descriptor;
	char hostname[SBUFSIZE];
	char* hostname_ptr = NULL;


	/* Open Input File, decrement our open requests counter */
	inputfp = fopen(*fname, "r");
	if(!inputfp){
	    fprintf(stderr, "Error Opening Input File: %s\n", *fname);
	    pthread_mutex_lock(&counter_mutex);
	    	open_requests--;
	    	pthread_mutex_unlock(&counter_mutex);
	    	return NULL;
	}	

	//read file
	while(fscanf(inputfp, INPUTFS, hostname) > 0)
	{

		pthread_mutex_lock(&queue_mutex);

		//if full, signal queue-fillers to wait
		while(queue_is_full(&q))
		{
			pthread_cond_wait(&queueNotFull, &queue_mutex);
		}

		//otherwise, continue to fill
		if (!queue_is_full(&q))
		{
			hostname_ptr = malloc(sizeof(hostname));
			strcpy(hostname_ptr, hostname);
			if (queue_push(&q, hostname_ptr) == QUEUE_FAILURE)
			{
				fprintf(stderr, "queue push failed!\n");
			}
			hostname_ptr = NULL;
		}
		else
		{
			printf("lost hostname");
		}
		pthread_mutex_unlock(&queue_mutex);
	}



	/* Close Input File */
	fclose(inputfp);
	pthread_mutex_lock(&counter_mutex);
	open_requests--;
	pthread_mutex_unlock(&counter_mutex);
	return NULL;

}

/* code for resolver threads */
void* resolve_IP_Name(void* file_descriptor)
{
	FILE** outputfp = file_descriptor;
	int queue_empty = 1;
	char* hostname_ptr = NULL;
	char firstipstr[MAX_IP_LENGTH];
	node* head_ptr;
	node* tmp_ptr;

	//if this queue isn't empty, or there are still threads waiting to go
	//lock, signal other threads to go, pop host name off queue and fetch its ip
	while(open_requests || !queue_empty)
	{
		pthread_mutex_lock(&queue_mutex);

		pthread_cond_signal(&queueNotFull);
		
		if(!(queue_empty = queue_is_empty(&q)))
		{
			if ((hostname_ptr = queue_pop(&q)) == NULL)
			{
				fprintf(stderr, "queue_pop failed\n");
			}
		}
		else 
		{
			pthread_mutex_unlock(&queue_mutex);
			continue;
		}

		pthread_mutex_unlock(&queue_mutex);

		head_ptr = malloc(sizeof(node));
		head_ptr -> link = NULL;
		//call head pointer to link multiple ip addresses together in linked list
		if(multidnslookup(hostname_ptr, head_ptr, sizeof(firstipstr)) == UTIL_FAILURE)
		{
			fprintf(stderr, "lookup error: %s\n", hostname_ptr);
			strncpy(firstipstr, "", sizeof(firstipstr));
		}


		//for each thread, write the hostname and all subsequent ip addresses
		pthread_mutex_lock(&write_mutex);
		fprintf(*outputfp, "%s",hostname_ptr);
		while(head_ptr->link != NULL)
		{
			fprintf(*outputfp, "%s",head_ptr->data);
			tmp_ptr = head_ptr;
			head_ptr = head_ptr -> link;
			free(tmp_ptr);
		}

		//so valgrind doesn't complain
		fprintf(*outputfp, "\n");
		free(head_ptr);
		free(hostname_ptr);
		pthread_mutex_unlock(&write_mutex);
	}
	return NULL;
}