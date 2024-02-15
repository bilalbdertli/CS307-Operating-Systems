#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <stdlib.h>
#include <cstdio>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>    

using namespace std;

// Define a Critical Region for printing to the console, which will be called by threads
void console_critical_region(int* fd, mutex& threadLock){
    FILE *fopen = fdopen(fd[0], "r"); //open read end of anonymous file, for reading
    char cp[2];
    threadLock.lock();
    cout << "----" << this_thread::get_id() << "\n";
    while(fgets(&cp[0], 2, fopen)){
        cout << cp;
    }
    cout << "----" << this_thread::get_id() << "\n";
    fflush(stdout);
    threadLock.unlock();
}


int main(){
    fstream inputfile("commands.txt"); //to read from 
    ofstream outputfile("parse.txt"); //
    vector<vector<string>> parsed_list;
    if(!inputfile.is_open()) cout << "File is not open, please check if it exists in this directory.\n";
    else{
        string line = "";
        while(getline(inputfile, line)){
            istringstream iss(line);
            vector<string> parsed; //for each parsed line result
            string cmd_name, input, option, isRedirected = "-", fileName="", isBackgroundJob = "n", remaining;
            while(iss >> remaining){
                if(remaining == "ls" || remaining == "wc" || remaining == "man" 
                    || remaining == "grep" || remaining == "wait"){
                        if(cmd_name == ""){cmd_name = remaining;}
                        else{input = remaining;}
                     
                    }               
                else if(remaining == "<" || remaining == ">"){
                    isRedirected = remaining;
                    iss >> fileName; //if there is redirection, then it is followed by a fileName for sure
                }
                else if(remaining[0] == '-' && remaining.length() == 2){ //if this is an option 
                    option = remaining;
                }
                else if(remaining == "&"){ //if this is a background job
                    isBackgroundJob = "y";
                }
                else{
                    input = remaining; //if none of the above, it is an input
                }
            }
            parsed.push_back(cmd_name);
            parsed.push_back(input);
            parsed.push_back(option);
            parsed.push_back(isRedirected);
            parsed.push_back(fileName);
            parsed.push_back(isBackgroundJob);
            parsed_list.push_back(parsed);

        }
        for(int i = 0; i < parsed_list.size(); i++){
            outputfile << "----------\n";
            outputfile << "Command: " << parsed_list[i][0] << "\n"; //0th is always cmd
            outputfile << "Inputs: " << parsed_list[i][1] << "\n";
            outputfile << "Options: " << parsed_list[i][2] << "\n";
            outputfile << "Redirection: " << parsed_list[i][3] << "\n";
            outputfile << "Background Job: " << parsed_list[i][5] << "\n"; // not 4, since it is reserved for file name
            outputfile << "----------\n";
        }

        // Now we have all of the commands in a vector called parsed_list
        mutex threadLock;
        vector<thread> shell_threads; // In case of a wait command, we need to join for background threads
        vector<int> backgroundTaskID; //To keep track of background processes, we need this list in case of wait command
        char* escape = strdup("-e");

        for(int i = 0; i < parsed_list.size(); i++){

            if(parsed_list[i][0] == "wait"){ //If the command is wait, wait for all background tasks
                for(int k = 0; k < backgroundTaskID.size(); k++){
                    waitpid(backgroundTaskID[k], NULL, 0);
                }
                backgroundTaskID.clear(); //Since all background tasks are terminated, free the list (FOR PROCESSES)
                for(int k = 0; k < shell_threads.size(); k++){
                    shell_threads[k].join();
                }
                shell_threads.clear(); //Since all background tsasks are terminated, free the list (FOR THREADS)
            }

            else if(parsed_list[i][3] == ">"){//If we have output redirectioning, no need to synchronize (out of scope of this homework)
                if(parsed_list[i][5] == "n"){ //If this is not a background task
                    int rc = fork();
                    if(rc<0){
                        fprintf(stderr, "fork failed\n");
                        exit(1);
                    }
                    else if (rc == 0){ //child process
                        close(STDOUT_FILENO);
                        int file = open(parsed_list[i][4].data(), O_CREAT | O_WRONLY, 0666);
                        vector<char*> execvp_params;
                        for(int k = 0; k < 3; k++){
                            if(parsed_list[i][k] != ""){
                                execvp_params.push_back(&parsed_list[i][k][0]);
                            }
                            if(k == 0 && parsed_list[i][k] == "grep"){
                                execvp_params.push_back(escape);
                            }
                        }
                        execvp_params.push_back(NULL);
                        char** execarray = execvp_params.data();
                        execvp(execarray[0], execarray);
                        
                    }
                    else{ // shell process
                        waitpid(rc, NULL, 0);
                    } 
                }

                else{ //if this is a background task
                    int rc = fork();
                    if(rc < 0){
                        fprintf(stderr, "fork failed\n");
                        exit(1);
                    }
                    else if (rc == 0){ //child process
                        close(STDOUT_FILENO);
                        int file = open(parsed_list[i][4].data(), O_CREAT | O_WRONLY, 0666);
                        vector<char*> execvp_params;
                        for(int k = 0; k < 3; k++){
                            if(parsed_list[i][k] != ""){
                                execvp_params.push_back(&parsed_list[i][k][0]);
                            }
                            if(k == 0 && parsed_list[i][k] == "grep"){
                                execvp_params.push_back(escape);
                            }
                        }
                        execvp_params.push_back(NULL);
                        char** execarray = execvp_params.data();
                        execvp(execarray[0], execarray);
                    }
                    else{
                        backgroundTaskID.push_back(rc);
                    }
                }
            }
            else{ // If we have input redirectioning, or no directioning, we use synchronization for processes and threads 
                int* fd = new int[2]; // Pipe creation for each new process to communicate with shell process
                pipe(fd);
                if(parsed_list[i][3] == "<"){ //If there is an input directioning
                    int rc2 = fork();
                    if(rc2 < 0){
                        fprintf(stderr, "fork failed\n");
                        exit(1);
                    }
                    else if (rc2 == 0){ // command process 
                        close(STDIN_FILENO); // Close since we will not write anything
                        open(parsed_list[i][4].data(), O_RDONLY); //Instead read from the given file
                        dup2(fd[1], STDOUT_FILENO); //Write to the write end of the file
                        vector<char*> execvp_params;
                        for(int k = 0; k < 3; k++){
                            if(parsed_list[i][k] != ""){
                                execvp_params.push_back(&parsed_list[i][k][0]);
                            }
                            if(k == 0 && parsed_list[i][k] == "grep"){
                                execvp_params.push_back(escape);
                            }
                        }
                        execvp_params.push_back(NULL);
                        char** execarray = execvp_params.data();
                        execvp(execarray[0], execarray);
                    }
                    else{ // shell process
                        close(fd[1]); //We will not write anything to the anonymous file, so we can close it
                        if(parsed_list[i][5] == "n"){ //If not a background process, just wait for it to finish
                            thread listener(console_critical_region, fd, ref(threadLock));
                            listener.join();
                        }
                        else{ // If background process, add it to the vector to use in case of wait command 
                            shell_threads.push_back(thread(console_critical_region, fd, ref(threadLock)));
                        }
                    }
                }

                else{ //If there is no redirectioning
                    int rc3 = fork();
                    if(rc3 < 0){
                        fprintf(stderr, "fork failed\n");
                        exit(1);
                    }
                    else if (rc3 == 0){ // command process
                        close(fd[0]); //We will not read from anonymous file
                        dup2(fd[1], STDOUT_FILENO); // Redirect output to the write end of the anonymous file
                        vector<char*> execvp_params;
                        for(int k = 0; k < 3; k++){
                            if(parsed_list[i][k] != ""){
                                execvp_params.push_back(&parsed_list[i][k][0]);
                            }
                            if(k == 0 && parsed_list[i][k] == "grep"){
                                execvp_params.push_back(escape);
                            }
                        }
                        execvp_params.push_back(NULL);
                        char** execarray = execvp_params.data();
                        execvp(execarray[0], execarray);
                    }
                    else{ // shell process
                        close(fd[1]); //We will not write anything to the anonymous file, so we can close it
                        if(parsed_list[i][5] == "n"){ //If not a background process, just wait for it to finish
                            thread listener(console_critical_region, fd, ref(threadLock));
                            listener.join();
                        }
                        else{ // If background process, add it to the vector to use in case of wait command 
                            shell_threads.push_back(thread(console_critical_region, fd, ref(threadLock)));
                        }
                        
                    }

                }
            }

            if(i == parsed_list.size() - 1){ //If all commands are executed, wait for all background tasks
                for(int k = 0; k < backgroundTaskID.size(); k++){
                    waitpid(backgroundTaskID[k], NULL, 0);
                }
                backgroundTaskID.clear(); //Since all background tasks are terminated, free the list (FOR PROCESSES)
                for(int k = 0; k < shell_threads.size(); k++){
                    shell_threads[k].join();
                }
                shell_threads.clear(); //Since all background tasks are terminated, free the list (FOR THREADS)
            }
        }
    }
    return 0;
}
