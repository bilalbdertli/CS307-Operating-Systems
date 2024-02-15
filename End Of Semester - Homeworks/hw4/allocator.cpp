#include <iostream>
#include <mutex>
#include <string>
#include <pthread.h>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
using namespace std;

struct Node {
    long ID;
    int SIZE;
    int INDEX;
    Node* next;

    Node(long id, int size, int index) : ID(id), SIZE(size), INDEX(index), next(nullptr) {}
};

class LinkedList {
public:
    Node* head;

public:
    LinkedList() : head(nullptr) {}
    LinkedList(int size){
        head = new Node(-1, size, 0); //Initialize the head with the given size, initially the list is empty (-1)
    }

    // Display the list
    void display() {
        Node* current = head;
        while (current != nullptr) {
            cout << "[" << current->ID << "][" << current->SIZE << "][" << current->INDEX <<"]";
            if(current->next != nullptr) cout << "---";
            current = current->next;
        }
        cout << "\n";
    }

    int allocateMemory(long threadID, int size){
        Node* current = head;
        while(current != nullptr){
            if(current -> ID == -1 && current -> SIZE >= size){
                if(current -> SIZE == size){
                    current -> ID = threadID;
                    return current -> INDEX;
                }
                else{
                    Node* newAllocated = new Node(-1, current -> SIZE - size, current -> INDEX + size);
                    current -> SIZE = size;
                    current -> ID = threadID;
                    newAllocated -> next = current -> next;
                    current -> next = newAllocated;
                    return current -> INDEX;
                }
            }
            current = current -> next;
        }
        return -1;
    }

    int deallocateMemory(long threadID, int index) {
        Node* current = head; //To traverse the tree
        Node* prev = nullptr; //For checking if the previous node is free, and merge them

        while (current != nullptr) {
            // Check if current node is allocated by the running thread
            if (current -> ID == threadID && current -> INDEX == index) { //If this is the true node to delete
                current -> ID = -1; // Mark as free (remains as this if neighbors are not free)

                // Merge with next free block if possible
                if (current -> next != nullptr && current -> next -> ID == -1) { //If there is a next node, and its free
                    Node* next = current -> next;
                    current -> SIZE += next -> SIZE; //Merge the two nodes by adding sizes, index remains the same
                    current -> next = next -> next; 
                    delete next; // Delete the merged node
                }

                // Merge with previous free block if possible
                if (prev != nullptr && prev -> ID == -1) { //If there is a previous node, and it is free
                    prev -> SIZE += current -> SIZE; //Add the size of current no next, to merge the two, index remains same
                    prev -> next = current -> next; //Link the previous node with the next node to unlink current (merged)
                    delete current; // Delete the merged node
                    //current = prev -> next;
                }

                return 1; // Deallocated successfully
            }

            // Move to the next node
            prev = current;
            current = current->next;
        }

        return -1; // Node with given threadID does not exist (this thread did not allocate any memory)
    }


    // Destructor to free the list
    /*~LinkedList() {
        while (head != nullptr) {
            Node* temp = head;
            head = head->next;
            delete temp;
        }
    }*/
};

class HeapManager {
private:
    LinkedList list;
    mutex managerLock; // Mutex for thread-safe operations

public:
    //Function that initializes the heap simulator with given size
    int initHeap(int size){
        //lock_guard<mutex> lock(managerLock); // Lock the entire list during the operation
        managerLock.lock();
        if(size <= 0){
            managerLock.unlock();
            return -1;
        } 
        else{
            list = LinkedList(size);
            cout << "Memory initialized\n";
            list.display();
            managerLock.unlock();
            return 1;
        }
    }

    int myMalloc(long ID, int size){
        //lock_guard<mutex> lock(managerLock); // Lock the entire list during the operation
        managerLock.lock();
        if(size <= 0){
            managerLock.unlock();
            return -1;
        }
        int isAlloc;
        isAlloc = list.allocateMemory(ID, size);
        if(isAlloc == -1 ) {
            cout << "Cannot allocate, requested size " << size << " for thread " << ID << " is bigger than the remaining size\n";
        }  //fail
        else{ //success
            cout << "Allocated for thread " << ID << "\n";
        }
        list.display();
        managerLock.unlock();
        return isAlloc;
    } 

     int myFree(long ID, int index){
        //lock_guard<mutex> lock(managerLock); // Lock the entire list during the operation
        managerLock.lock();
        if(index < 0){
            managerLock.unlock();
            return -1;}
        int isDealloc;
        isDealloc = list.deallocateMemory(ID, index);
        if(isDealloc == -1 ) {
            cout << "Cannot free, requested index " << index << " for thread " << ID << " does not exist in the heap\n";
        }  //fail
        else{ //success
            cout << "Freed for thread " << ID << "\n";
        }
        list.display();
        managerLock.unlock();
        return isDealloc;
    } 

    //printf("Thread ID: %ld,\n", pthread_self());

     // Display the list
    void print() {
        //lock_guard<mutex> lock(managerLock); // Lock the entire list during the operation
        managerLock.lock();
        list.display();
        managerLock.unlock();
    }
};
