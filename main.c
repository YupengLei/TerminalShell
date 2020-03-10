/*
 Copyright @ Yupeng Lei

 Licensed under the MIT License

 This is a terminal shell in C that
 support exit;
 support show;
 support team;
 support export, unexport, set;
 support wait;
 support External executable instruciton;
 support CTRL_C handler;
 support comment '#';
 support pid variable $$, last background pid $!;
 support stdin/stdout redirection '<' '>';
 support pipe '|';
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define BUFLEN 128
#define INSNUM 8
/*internal instructions*/
char *instr[INSNUM] = {"show","set","export","unexport","show","exit","wait","team"};
/*predefined variables*/
/*varvalue[0] stores the rootpid of xssh*/
/*varvalue[3] stores the childpid of the last process that was executed by xssh in the background*/
int varmax = 3;
char varname[BUFLEN][BUFLEN] = {"$\0", "?\0", "!\0",'\0'};
char varvalue[BUFLEN][BUFLEN] = {'\0', '\0', '\0'};
/*remember pid*/
int childnum = 0;
pid_t childpid = 0;
pid_t rootpid = 0;
/*current dir*/
char rootdir[BUFLEN] = "\0";

/*functions for parsing the commands*/
int deinstr(char buffer[BUFLEN]);
void substitute(char *buffer);
char *trim(char *s);
int xsshexit(char buffer[BUFLEN]);
void show(char buffer[BUFLEN]);
void team(char buffer[BUFLEN]);
int program(char buffer[BUFLEN]);
void ctrlsig(int sig);
void waitchild(char buffer[BUFLEN]);
void set(char buffer[BUFLEN]);
void export(char buffer[BUFLEN]);
void unexport(char buffer[BUFLEN]);
int pipeprog(char buffer[BUFLEN]);
void redirectprog(char bufer[BUFLEN]);

/*main function*/
int main() {
	/*set the variable $$*/
	rootpid = getpid();
	childpid = rootpid;
	sprintf(varvalue[0], "%d", rootpid);

	/*capture the ctrl+C*/
	if(signal(SIGINT, ctrlsig) == SIG_ERR) {
		printf("-xssh: Error on signal ctrlsig\n");
		exit(0);
	}

	/*run the xssh, read the input instrcution*/
	int xsshprint = 0;
	if(isatty(fileno(stdin))) xsshprint = 1;
	if(xsshprint) printf("xssh>> ");
	char buffer[BUFLEN];

	while(fgets(buffer, BUFLEN, stdin) > 0) {
        /*substitute the variables*/
		substitute(buffer);
		/*delete the comment*/
		char *p = strchr(buffer, '#');
		if(p != NULL) {
			*p = '\0';
            *(p+1) = '\0';
		}

		/*decode the instructions*/
		int ins = deinstr(buffer);
		/*run according to the decoding*/
		if(ins == 1)
			show(buffer);
		else if(ins == 2)
			set(buffer);
		else if(ins == 3)
			export(buffer);
		else if(ins == 4)
			unexport(buffer);
		else if(ins == 5) show(buffer); //Not used for now
		else if(ins == 6)
			return xsshexit(buffer);
		else if(ins == 7)
			waitchild(buffer);
		else if(ins == 8)
			team(buffer);
		else if(ins == 9)
			continue;
		else {
			char *ptr = strchr(buffer, '|');
            char *ptr2= strchr(buffer, '<');
            char *ptr3= strchr(buffer, '>');

			if(ptr != NULL) {
				int err = pipeprog(buffer);
				if(err != 0)break;
			}
            else if (ptr2 != NULL || ptr3 != NULL) {
                redirectprog(buffer);
            }
			else {
                int err = program(buffer);
                if(err != 0)break;
			}
		}
		if(xsshprint) printf("xssh>> ");
		memset(buffer, 0, BUFLEN);
	}
	return -1;
}

/*exit I*/
int xsshexit(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int len, i, status, start;
    start = 5;
    len = strlen(buffer+start)-1;
    status = 0;

    for (i=0; i<len; i++) {
        status = status * 10 + (buffer[start+i] - '0');
    }

    if (status<256 && status >= 0) {
        exit(status);
    } else {
        exit(-1);
    }
}

/*show W*/
void show(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int start = 5;
    printf("%s\n", buffer + start);
}

/*team T*/
void team(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    printf("teammembers: Yupeng Lei\n");
}

/*export variable --- set the variable name in the varname list*/
void export(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int i, j;
    //flag == 1, if variable name exists in the varname list
    int flag = 0;
    //parse and store the variable name in buffer[]
    char str[BUFLEN];
    int start = 7;
    while(buffer[start]==' ') start++;

    for(i = start; (i < strlen(buffer))&&(buffer[i]!='#')&&(buffer[i]!=' ')&&(buffer[i]!='\n'); i++) {
                str[i-start] = buffer[i];
    }

    str[i-start] = '\0';

    for (j = 0; j < varmax; j++) {
        int res = strcmp(str, varname[j]);
        if (res == 0) {
            flag = 1;
            break;
        }
    }

    if(flag == 0) {
            strcpy(varname[varmax], str);
            varmax ++;
            strcpy(varvalue[varmax], "");
            printf("-xssh: Export variable %s.\n", varname[varmax-1]);

    } else { //variable name already exists in the varname list
        printf("-xssh: Existing variable %s is %s.\n", varname[j], varvalue[j]);
    }
}

/*unexport the variable --- remove the variable name in the varname list*/
void unexport(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int i, j;
    //flag == 1, if variable name exists in the varname list
    int flag = 0;
    //parse and store the variable name in buffer[]
    char str[BUFLEN];
    int start = 9;
    while(buffer[start]==' ') start++;

    for(i = start; (i < strlen(buffer))&&(buffer[i]!='#')&&(buffer[i]!=' ')&&(buffer[i]!='\n'); i++) {
        str[i-start] = buffer[i];
    }

    str[i-start] = '\0';

    for(j = 0; j < varmax; j++) {
        int res = strcmp(str, varname[j]);
        if (res == 0) {
                flag = 1;
                break;
        }
    }

    if(flag == 0) {  //variable name does not exist in the varname list
        printf("-xssh: Variable %s does not exist.\n", str);
    } else {//variable name already exists in the varname list
        strcpy(varname[j], "");
        strcpy(varvalue[j], "");
        printf("-xssh: Variable %s is unexported.\n", str);
    }
}

/*set the variable --- set the variable value for the given variable name*/
void set(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int i, j;
    //flag == 1, if variable name exists in the varname list
    int flag = 0;
    //parse and store the variable name in buffer[]
    char str[BUFLEN];
    int start = 4;
    while(buffer[start]==' ') start++;

    for(i = start; (i < strlen(buffer))&&(buffer[i]!=' ')&&(buffer[i]!='#') && (buffer[i]!='\n'); i++) {
        str[i-start] = buffer[i];
    }

    str[i-start] = '\0';
    while(buffer[i]==' ') i++;

    if(buffer[i]=='\n') {
        printf("No value to set!\n");
        return;
    }

    for(j = 0; j < varmax; j++) {
        int res = strcmp(str, varname[j]);
        if (res == 0) {
            flag = 1;
            break;
        }
    }

    if(flag == 0) {
        printf("-xssh: Variable %s does not exist.\n", str);
    } else {
        strcpy(varvalue[j], buffer + i);
        printf("-xssh: Set existing variable %s to %s.\n", varname[j], varvalue[j]);
    }
}


/*ctrl+C handler*/
void ctrlsig(int sig) {
    int rootpidNum=atoi(varvalue[0]);
    if(sig == SIGINT) {
        if(childpid != rootpidNum) {
            printf("-xssh: Exit pid %d.\n", childpid);
            fflush(stdout);
            kill(childpid, SIGKILL);
        } else {
            printf(" this is -xssh itself, ctrl+c is not working.\n");
            printf("xssh>>");
            fflush(stdout);
        }
        childpid = rootpidNum;
    }
}

/*wait instruction*/
void waitchild(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int i;
    int start = 5;
    int status;
    /*store the childpid in pid*/
    char number[BUFLEN] = {'\0'};
    while(buffer[start]==' ') start++;

    for(i = start; (i < strlen(buffer))&&(buffer[i]!='\n')&&(buffer[i]!='#'); i++) {
        number[i-start] = buffer[i];
    }

    number[i-start] = '\0';
    char *endptr;
    int pid = strtol(number, &endptr, 10);

    /*simple check to see if the input is valid or not*/
    if((*number != '\0')&&(*endptr == '\0')) {
        if(pid != -1) {
            int wpid = waitpid(pid, &status, 0);
            if(wpid > 0) {
                printf("-xssh: Have finished waiting process %d.\n", pid);
                childnum --;
            } else {
                printf("-xssh: Unsuccessfully wait the background process %d.\n", pid);
            }
        } else {
            printf("-xssh: wait %d background processes.\n", childnum);
            waitpid(-1, &status, 0);
        }
    } else {
        printf("-xssh: wait: Invalid pid.\n");
    }
    childnum=0;
}

/*execute the external command*/
int program(char buffer[BUFLEN]) {
    /*if backflag == 0, xssh need to wait for the external command to complete*/
    /*if backflag == 1, xssh need to execute the external command in the background*/
    while(isspace(*buffer)) buffer++;
    int backflag = 0;
    char *ptr = strchr(buffer, '&');
    if(ptr != NULL) {
        backflag = 1;
        childnum ++;
    }
    pid_t pid, wpid;
    int status;
    pid = fork();
    childpid = pid;
    sprintf(varvalue[2], "%d", pid);
    char *space = " ";
    char *parsed;
    char **command= malloc(8* sizeof(char *));
    int i = 0;
    parsed = strtok(buffer, space);
    while (parsed != NULL) {
        parsed[strlen(parsed)]='\0';
        command[i] = parsed;
        i++;
        parsed = strtok(NULL, space);
    }
    command[i++] = parsed;

    if(backflag==0) {
        if(pid<0) {
            printf("Failed to fork.\n");
            return -2;
        }
        if(pid==0) {
            int exec_return=execvp(command[0], command);
            if(exec_return<0) {
                printf("Unable to execute.\n");
                exit(exec_return);
            }
        }
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    else if (backflag==1) {
        if(pid<0) {
            printf("Failed to fork.\n");
            return -2;
        }
        if (pid==0) {
            int exec_return=execvp(command[0], command);
            if(exec_return<0) {
                printf("Unable to execute.\n");
                exit(exec_return);
            }
        }
    }

    return 0;
}

/*execute the pipe programs*/
int pipeprog(char buffer[BUFLEN])
{
    while(isspace(*buffer)) buffer++;
    char *verticalbar = "|";
    char **command= malloc(8* sizeof(char *));
    int i = 0;
    char *parsed, *rfile[1000], *lfile[1000];
    int rightargnum=0, leftargnum=0;
    char *rightcmdArgs[1000], *leftcmdArgs[1000];
    int status1, status2;
    parsed = strtok(buffer, verticalbar);
    while (parsed != NULL) {
        parsed[strlen(parsed)]='\0';
        rfile[i] = parsed;
        i++;
        parsed = strtok(NULL, verticalbar);
    }
    rfile[i]=NULL;
    rfile[0]=trim(rfile[0]);
    rfile[1]=trim(rfile[1]);
    parsed=strtok(rfile[0], " ");

    while(parsed!=NULL) {
        parsed[strlen(parsed)]='\0';
        leftcmdArgs[leftargnum]=parsed;
        leftargnum++;
        parsed=strtok(NULL, " ");
    }

    leftcmdArgs[leftargnum]=NULL;
    leftcmdArgs[0]=trim(leftcmdArgs[0]);
    parsed=strtok(rfile[1], " ");

    while(parsed!=NULL) {
        parsed[strlen(parsed)]='\0';
        rightcmdArgs[rightargnum]=parsed;
        rightargnum++;
        parsed=strtok(NULL, " ");
    }

    rightcmdArgs[rightargnum]=NULL;
    rightcmdArgs[0]=trim(rightcmdArgs[0]);
    int fd[2];
    int status;
    pid_t pid1,pid2;

    if (pipe(fd)==-1) {
        printf("pipe failed\n");
    }

    pid1=fork();
    if(pid1 == 0) {
        close(fd[0]);
        dup2(fd[1], 1);
        int exec_return=execvp(leftcmdArgs[0], leftcmdArgs);
        if(exec_return<0) {
            printf("failed to execute first.\n");
        }
        return 0;
    } else {
        pid2=fork();
        if(pid2==0) {
            close(fd[1]);
            dup2(fd[0], 0);
            int exec_return=execvp(rightcmdArgs[0], rightcmdArgs);
            if(exec_return<0) {
                printf("failed to execute first.\n");
            }
            return 0;
        } else {
            close(fd[1]);
            close(fd[0]);
            waitpid(pid1, &status1, WUNTRACED);
            waitpid(pid2, &status2, WUNTRACED);
            return 0;
        }
    }
    return 0;
}

char *trim (char *s) {
    int i;
    while (isspace (*s)) s++;   // skip left side white spaces
    for (i = strlen (s) - 1; (isspace (s[i])); i--) ;   // skip right side white spaces
    s[i + 1] = '\0';
    return s;
}

void redirectprog(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int outnum=0;
    int innum=0;
    int argnum=0;
    int in=0;
    int out=0;
    char *parsed, *rfile[1000], *cmdArgs[1000], *lfile[1000];
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    pid_t pid;
    parsed=strtok(buffer, ">");

    while(parsed!=NULL) {
        parsed[strlen(parsed)]='\0';
        rfile[outnum]=parsed;
        outnum ++;
        parsed=strtok(NULL, ">");
    }

    rfile[outnum]=NULL;
    rfile[1]=trim(rfile[1]);
    parsed=strtok(rfile[0], "<");

    while(parsed!=NULL) {
        parsed[strlen(parsed)]='\0';
        lfile[innum]=parsed;
        innum++;
        parsed=strtok(NULL, "<");
    }

    lfile[innum]=NULL;
    lfile[1]=trim(lfile[1]);
    parsed=strtok(lfile[0], " ");

    while(parsed!=NULL) {
        parsed[strlen(parsed)]='\0';
        cmdArgs[argnum]=parsed;
        argnum++;
        parsed=strtok(NULL, " ");
    }

    cmdArgs[argnum]=NULL;
    pid=fork();

    if(pid<0) {
        printf("failed to create child process.\n");
    }

    if(pid==0) {
        //just input < redirection
        if(innum>1 && outnum==1) {
            in = open(lfile[1], O_CREAT | O_RDONLY, mode);
            if (in<0) {
                printf("%s doesn't exist.\n", lfile[1]);
                exit(1);
            }
            dup2(in, 0);
            close(in);
            if(execvp(cmdArgs[0], cmdArgs)<0) {
                printf("failed to excecute.\n");
            }
        }
        //just output > redirection
        if(outnum>1 && innum==1) {
            printf("dddd\n");
            out=open(rfile[1], O_CREAT | O_WRONLY, mode);
            if (out<0) {
                printf("%s doesn't exist.\n", rfile[1]);
                exit(1);
            } else {
                dup2(out, 1);
                close(out);
                if(execvp(cmdArgs[0], cmdArgs)<0) {
                    printf("failed to excecute.\n");
                }
            }
        }

        //both input and output
        if(innum>1 && outnum >1) {
            int out,in;
            out = open(rfile[1], O_CREAT | O_WRONLY, mode);
            in = open(lfile[1], O_CREAT | O_RDONLY , mode);
            if (out < 0) {
                printf("%s doesn't exist.\n", rfile[1]);
                exit(1);
            }
            if (in < 0) {
                printf("%s doesn't exist.\n", lfile[1]);
                exit(1);
            } else {
                dup2(in, 0);
                dup2(out, 1);
                close(in);
                close(out);
                if(execvp(cmdArgs[0], cmdArgs)<0) {
                    printf("failed to excecute.\n");
                }
            }
        }
    }
    waitpid(pid, NULL, 0);
}

/*substitute the variable with its value*/
void substitute(char *buffer) {
    char newbuf[BUFLEN] = {'\0'};
    int i;
    int pos = 0;
    while(isspace(*buffer)) buffer++;

    for(i = 0; i < strlen(buffer);i++) {
        if(buffer[i]=='#') {
            newbuf[pos]='\n';
            pos++;
            break;
        }
        else if(buffer[i]=='$') {
            if((buffer[i+1]!='#')&&(buffer[i+1]!=' ')&&(buffer[i+1]!='\n')) {
                i++;
                int count = 0;
                char tmp[BUFLEN];
                for(; (buffer[i]!='#')&&(buffer[i]!='\n')&&(buffer[i]!=' '); i++) {
                    tmp[count] = buffer[i];
                    count++;
                }
                tmp[count] = '\0';
                int flag = 0;
                int j;
                for(j = 0; j < varmax; j++) {
                    if(strcmp(tmp,varname[j]) == 0) {
                        flag = 1;
                        break;
                    }
                }
                if(flag == 0) {
                    printf("-xssh: Does not exist variable $%s.\n", tmp);
                } else {
                    strcat(&newbuf[pos], varvalue[j]);
                    pos = strlen(newbuf);
                }
                i--;
            } else {
                newbuf[pos] = buffer[i];
                pos++;
            }
        } else {
            newbuf[pos] = buffer[i];
            pos++;
        }
    }

    newbuf[pos-1] = '\0';
    strcpy(buffer, newbuf);
}


/*decode the instruction*/
int deinstr(char buffer[BUFLEN]) {
    while(isspace(*buffer)) buffer++;
    int i;
	int flag = 0;
	for(i = 0; i < INSNUM; i++) {
		flag = 0;
		int j;
		int stdlen = strlen(instr[i]);
		int len = strlen(buffer);
		int count = 0;
		j = 0;
		while(buffer[count]==' ') count++;

		if((buffer[count]=='\n')||(buffer[count]=='#')) {
			flag = 0;
			i = INSNUM;
			break;
		}

		for(j = count; (j < len)&&(j-count < stdlen); j++) {
			if(instr[i][j] != buffer[j]) {
				flag = 1;
				break;
			}
		}

		if((flag == 0) && (j == stdlen) && (j <= len) && (buffer[j] == ' ')) {
			break;
		}
		else if((flag == 0) && (j == stdlen) && (j <= len) && (i == 5)) {
			break;
		}
		else if((flag == 0) && (j == stdlen) && (j <= len) && (i == 7)) {
			break;
		} else {
			flag = 1;
		}
	}

	if(flag == 1) {
		i = 0;
	} else {
		i++;
	}
	return i;
}
