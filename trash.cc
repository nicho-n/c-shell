
/*

Nick Nowak's Shell, December 2017

1  - Can run executable
1  - Search path for exectuable
1  - Prompt has username
1  - Control+L clears the screen
1  - Queue commands
1  - Lots of semicolons
2  - Tab completion and arrow history
2  - runs executables from approved list
2  - automatically runs .trash when it starts.
2  - can run commands from a file.
3  - run commands in the background
2  - expand environmental variables
2  - change prompt

......
21 pts

*/

#include <sys/mman.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include "TrashFile.h"
#include <fcntl.h>
#include <cstring>
#include "trash.h"
#include <sys/wait.h>
#include <stdio.h>
#include <cstdlib>
#include <readline/readline.h>
#include <readline/history.h>
using namespace std;

string prompt = (string(getenv("USER")) + string("@trash.cc:~$ "));

//returns a null-terminated vector in which each element is a token of the given string.
vector<const char *> parseCommand(const char * command, const char * delim){
        vector<const char *> v;
        char * dup = strdup(command);

        const char * tok = strtok(dup, delim);
        while(tok != NULL){
                v.push_back((const char *)tok);
                tok = strtok(NULL, delim);
        }

        v.push_back(NULL);
        return v;
}

//returns a vector in which each element is a directory of PATH.
vector<const char *> getPath(){
        extern char ** environ; const char * path; vector<const char *> p;
        for (int i = 0; environ[i] != NULL; i++)
                if (strstr(environ[i], "PATH=") != NULL){
                        path = (environ[i] += 5);
                        p  = parseCommand(path, ":");
                }
        return p;
}

//returns a valid path to an executable if it exists.
const char * pathTo(const char * command){
        vector<const char *> p = getPath();
        for (int i = 0; i < p.size()-1; i++){
                const char * executable = ((string(p[i]) + "/" + string(command)).c_str());
                if (access(executable, F_OK) == 0)
                        return executable;
        }
        return command;
}

void execStuff(vector<const char *> cmd){
        cmd[0] = pathTo(cmd[0]);
        const char ** c = &cmd[0];
        if(execv(c[0], (char * const *)c) == -1)
                perror("oops");
}

//runs in background if bg is true
void run(vector<const char *> cmd, bool bg){
        int pid = fork(); int status;
        if (pid == 0){
                if (strstr(cmd[0], "./")){
                        const char * file = (string(cmd[0]).substr(2, sizeof(cmd))).c_str();
                        if (access(file, F_OK) == 0) runFile(file);
                        else cout << "oops: No such file or directory" << endl;
                }
                else execStuff(cmd);
                exit(1);
        }
        else if (!bg)
                pid_t wpid = waitpid(pid, &status, WUNTRACED);
        else
                cout << pid << endl;
}

vector<string> loadFile(const char * filename){
        vector<string> v; string line;
        ifstream f(filename);
        while (getline(f, line))
                v.push_back(line);
        return v;
}

void runFile(string filename){
        vector<string> script = loadFile(filename.c_str());
        for (int i = 0; i < script.size(); i++){
                vector< vector<const char *> > cL = generateCommandList(script[i].c_str());
                runCommandList(cL);
        }
}

void runCommandList(vector < vector<const char *> > cmdList){
        auto  approved_commands = loadFile("approved_commands.txt");
        for (int i = 0; i < cmdList.size(); i++){
                        const char * n = parseCommand(cmdList[i][0], " ")[0];
                        if (contains(approved_commands, n) || cmdList[i][0][0] == '.'){
                                if (search(cmdList[i], "&"))
                                        run(removeAmpersand(cmdList[i]), 1);
                                else run(cmdList[i], 0);
                        }
                        else 
                                cout << "Invalid command." << endl;
        }

}

const char * removeFirstChar(const char * command){
        return command+1;
}

vector<const char *> expandEnv(vector<const char *> cmd){
        for (int i = 0; i < cmd.size()-1; i++)
                if (cmd[i][0] == '$'){
                        const char * n = getenv(removeFirstChar(cmd[i]));
                        if (n != NULL)
                                cmd[i] = n;
                }
        return cmd;
}

vector< vector<const char *> > generateCommandList(const char * command){
        vector<const char *> cmd;
        vector < vector<const char *> > cmdList;

        cmd.push_back(NULL);

        if (strstr(command, ";"))
                cmd = parseCommand(command, ";");
        if (cmd[0] == NULL)
                cmd = parseCommand(command, "");

        for (int i = 0; i < cmd.size()-1; i++){
                vector<const char *> c = parseCommand(cmd[i]," ");
                cmdList.push_back(expandEnv(c));
        }

        return cmdList;
}

//removes the second-to-last element of cmd
vector<const char *> removeAmpersand(vector<const char *> cmd){
        cmd[(cmd.size()-2)] = NULL;
        return cmd;
}

//returns true if s is the second-to-last element of v
bool search(vector<const char *> cmd, string s){
        for (int i = 0; i < cmd.size()-1; i++)
                if (cmd[i] == s)
                        return (cmd.size() - i == 2);
        return false;
}

bool contains(vector<string> cmds, string s){
        for (int i = 0; i < cmds.size(); i++)
                if (cmds[i] == s)
                        return true;
        return false;
}

void upArr(){
        if  (where_history() > 0)
                rl_replace_line(previous_history()->line, 0);

        rl_point = rl_end;
}

void downArr(){
        if (where_history() < history_length - 1)
                rl_replace_line(next_history()->line, 0);
        else
                rl_replace_line("", 0);

        rl_point = rl_end;
}

void writeHistory(const char * command, const char * filename){
        if (command[0] != '\0'){
                add_history(command);
                write_history(filename);
        }
}

int main(){
        signal(SIGCHLD, SIG_IGN);

        rl_bind_keyseq("\\e[A", (rl_command_func_t *)upArr);
        rl_bind_keyseq("\\e[B", (rl_command_func_t *)downArr);

        runFile(".trash");
        read_history(".trashhistory");

        while(1){
                int status; vector< vector<const char *> > commandList;
                const char * command = readline(prompt.c_str());
                writeHistory(command, ".trashhistory");
                if (string(command).substr(0,4) == "PS1=")
                        prompt = parseCommand(command,"PS1=")[0];
                else{
                        commandList = generateCommandList(command);
                        runCommandList(commandList);
                }
        }
}






