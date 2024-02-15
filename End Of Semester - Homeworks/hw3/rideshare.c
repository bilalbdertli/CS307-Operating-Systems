#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

int waitingA = 0, waitingB = 0, carNo = 0; //Initializing the global values for future use
pthread_mutex_t fanLock; //To lock while a fan is printing, to prevent data race on shared variables(int values above)
pthread_barrier_t printMidMsg; //To sync printf sections, will be used for each band of 4 threads
sem_t fansA, fansB; //Two distinct semaphores for both teams, explicit queue for waiting threads in it


void* newFanA(){
    pthread_mutex_lock (&fanLock); //Critical region begins here
    printf("Thread ID: %ld, Team: A, I am looking for a car.\n", pthread_self());
    bool isDriver = false; // Initially assume that this thread is not the last to form a band (so not the driver)
    if(waitingA == 3){ //If there are 3 waiting A threads, currently running thread will form a band with them.
        isDriver = true; //This last thread declares itself as the driver of the band
        waitingA -= 3; //Since we wake 3 A fans up
        sem_post(&fansA); //Wake up 3 threads from the queue to form a band
        sem_post(&fansA);
        sem_post(&fansA);
    }
    else if(waitingA >= 1 && waitingB >= 2){ //Other possibility: 2 A 2 B will form a band
        isDriver = true; //This last thread declares itself as the driver of the band
        waitingA -= 1; //Since we wake 1 A fan up
        waitingB -= 2; //Since we wake 2 B fans up
        sem_post(&fansA); //Wake 1 A fan and 2 B fans to form a band
        sem_post(&fansB);
        sem_post(&fansB);
    }
    else{ //Cannot form a valid combination: wait for another thread to form a band
        waitingA++; // This thread will be waiting for another thread to form a band
        pthread_mutex_unlock(&fanLock); //Release the lock before going into sleep
        sem_wait(&fansA); //Sleep i.e. wait for another thread to form a band
    }
    // This part will be entered by A fans after agreement on formation of a band.
    printf("Thread ID: %ld, Team: A, I have found a spot in a car.\n", pthread_self());

    pthread_barrier_wait(&printMidMsg); //Make sure that all 4 fans of a ride prints the mid message e.g. the message above
    if(isDriver){ //After all threads print that they found a car, driver prints that he/she is driving
        printf("Thread ID: %ld, Team: A, I am the captain and driving the car with ID %d.\n", pthread_self(), carNo);
        carNo++;
        pthread_mutex_unlock(&fanLock); //Release the lock before returning
    }
    return NULL;
}

void* newFanB(){
    pthread_mutex_lock (&fanLock); //Critical region begins here
    printf("Thread ID: %ld, Team: B, I am looking for a car.\n", pthread_self());
    bool isDriver = false; // Initially assume that this thread is not the last to form a band (so not the driver)
    if(waitingB == 3){ //If there are 3 waitingB threads, currently running thread will form a band with them.
        isDriver = true; //This last thread declares itself as the driver of the band
        waitingB -= 3; //Since we wake 3 B fans up
        sem_post(&fansB); //Wake up 3 threads from the queue to form a band
        sem_post(&fansB);
        sem_post(&fansB);
    }
    else if(waitingB >= 1 && waitingA >= 2){ //Other possibility: 2 A 2 B will form a band
        isDriver = true; //This last thread declares itself as the driver of the band
        waitingB -= 1; //Since we wake 1 B fan up
        waitingA -= 2; //Since we wake 2 A fans up
        sem_post(&fansB); //Wake 1 B fan and 2 A fans to form a band
        sem_post(&fansA);
        sem_post(&fansA);
    }
    else{ //Cannot form a valid combination: wait for another thread to form a band
        waitingB++; // This thread will be waiting for another thread to form a band
        pthread_mutex_unlock(&fanLock); //Release the lock before going into sleep
        sem_wait(&fansB); //Sleep i.e. wait for another thread to form a band
    }
    // This part will be entered by B fans after agreement on formation of a band.
    printf("Thread ID: %ld, Team: B, I have found a spot in a car.\n", pthread_self());

    pthread_barrier_wait(&printMidMsg); //Make sure that all 4 fans of a ride prints the mid message e.g. the message above
    if(isDriver){ //After all threads print that they found a car, driver prints that he/she is driving
        printf("Thread ID: %ld, Team: B, I am the captain and driving the car with ID %d.\n", pthread_self(), carNo);
        carNo++;
        pthread_mutex_unlock(&fanLock); //Release the lock before returning
    }
    return NULL;
}


int main(int argc, char *argv[]){
    if(argc !=3){
        printf("Usage: %s <numA> <numB>\n", argv[0]);
        return 1;
    }
    else{
        long numA, numB;
        char *endptrA, *endptrB;
        char *firstVal, *secondVal;
        firstVal = argv[1];
        secondVal = argv[2];
        errno = 0; //We will check for errno to see if there are any errors with the number 
        numA = strtol(firstVal, &endptrA, 10); //Convert the input into number (base 10)
        if(endptrA == firstVal || errno == ERANGE || *endptrA != '\0'){ //If the conversion could not be done because of noninteger input
            printf("The main terminates\n");
        }
        else{
            errno = 0;
            numB = strtol(secondVal, &endptrB, 10); //Convert the input into number (base 10)
            if(endptrB == secondVal || errno == ERANGE || *endptrB != '\0'){ //If the conversion could not be done because of noninteger input
                printf("The main terminates\n");
            }
            else{
                if(numA % 2 != 0 || numB % 2 != 0 || (numA + numB) % 4 != 0){ //Validation check for number of fans
                    printf("The main terminates\n");
                }
                //New additions start
                else if(numA < 0 || numB < 0){
                    printf("The main terminates\n");
                }
                //New additions end
                else{ //If the given numbers are valid
                    sem_init(&fansA,0,0); //Initialize the semaphores with initial value 0 (3rd parameter)
                    sem_init(&fansB,0,0);
                    //To make sure that no thread continues before all 4 threads print that they found a car to travel
                    pthread_barrier_init(&printMidMsg, NULL, 4); 
                    pthread_t *allFans = (pthread_t*) malloc ((numA+numB) * sizeof(pthread_t)); //Dynamic memory alloc for threads
                    //Thread creation, they will call a function according to their team (e.g. 2 6 -> 2 threads call funA, 6 will call funB)
                    for(int i = 0; i < numA; i++){
                        pthread_create(&allFans[i], NULL, newFanA, NULL);
                    }
                    for(int i = numA; i < (numA + numB); i++){
                        pthread_create(&allFans[i], NULL, newFanB, NULL);
                    }
                    //Wait for all threads
                    for(int k = 0; k < numA + numB; k++){
                        pthread_join(allFans[k], NULL); //Wait for all threads in allFans before terminating
                    }
                    free(allFans); //Free up the memory
                    pthread_barrier_destroy(&printMidMsg); //Free up the memory for the barrier as well
                    printf("The main terminates\n");

                } 
            }
        }
        
        return 0;
    }
}