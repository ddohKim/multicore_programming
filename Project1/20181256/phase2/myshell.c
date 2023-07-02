/* $begin shellmain */
#include "myshell.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXARGS 128

FILE *file;

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
char latest[200];
int late = 0;
char history2[10] = "/history"; //history 저장하는 root
char historyFile[200];
int main()
{
    getcwd(historyFile, 200); //현재 main이 실행되는 directory root 에서 history를 가져온다
    strcat(historyFile, history2);
    char cmdline[MAXLINE]; /* Command line */
    while (1)
    {
        /* Read */
        printf("CSE4100-MP-P1> ");
        fgets(cmdline, MAXLINE, stdin);

        if (feof(stdin))
            exit(0);

        /* Evaluate */

        eval(cmdline);
    }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */

void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    file = fopen(historyFile, "a+");
    late = 0;
    while (EOF != fscanf(file, "%[^\n]\n", latest))
    {
        late++;
    }
    strcat(latest, "\n");
    fclose(file); //가장 최근 History 내역을 가져온다

    int check_history = 0;
    for (int i = 0; i > -1; i++)
    {

        if (i == strlen(cmdline) - 1)
            break;
        if (cmdline[i] == '!' && cmdline[i + 1] == '!')
        { //!가 있는지 없는지  탐색
            check_history = 1; //if 문안에 들어왔기 때문에 있다고 판단
            if (late == 0)
            { //가장 최근 내역이 없을 때
                printf("No history.\n");
                return;
            }
            file = fopen(historyFile, "a+");
            char temp[200];
            while (EOF != fscanf(file, "%[^\n]\n", temp))
            {
            }//가장 최근 내역을 cmdline으로 바꾼다
            fclose(file);
            char str[200] = {};
            strncpy(str, cmdline, i);

            strcat(str, temp);
            strcat(str, cmdline + i + 2);
            strcpy(cmdline, str);
            i = 0;
        }
        if (cmdline[i] == '!' && cmdline[i + 1] >= 48 && cmdline[i + 1] <= 57)
        {
            check_history = 1;
            int temp_index = i + 1;
            char temp_history[200];
            int num = 0;
            while (cmdline[temp_index] >= 48 && cmdline[temp_index] <= 57)
            {

                temp_history[num] = cmdline[temp_index];
                temp_index++;
                num++;
            }

            if (late < (atoi(temp_history)))
            { //!# 이 history 숫자보다 클때
                printf("Invalid command number.\n");
                return;
            }

            file = fopen(historyFile, "a+");
            char temp[200];
            char history[200];
            int count = 1;
            while (EOF != fscanf(file, "%[^\n]\n", temp))
            {
                strcpy(history, temp);
                if (count == atoi(temp_history))
                {
                    break;
                }
                count++;
            }
            char str[200] = {};

            strncpy(str, cmdline, i);

            strcat(str, history);
            strcat(str, cmdline + i + num + 1);

            strcpy(cmdline, str); //cmdline을 바꿔준다
            i = 0;
        }
    }

    strcpy(buf, cmdline);
    char pipe_command[200][200] = {};

    int numofpipe = 0;

    for (int i = 0; i < strlen(cmdline); i++) // pipe 개수 찾아보기
    {
        if (cmdline[i] == '|')
        {
            numofpipe++;
        }
    }
    for (int i = 0; i < numofpipe; i++) // pipe parsing
    {
        strncpy(pipe_command[i], buf, strchr(buf, '|') - buf);
        strcat(pipe_command[i], " ");
        strcpy(buf, strchr(buf, '|') + 1); //buf에 pipe parsing 한 정보들을 적어준다 
    }
    strcat(pipe_command[numofpipe], buf); //마지막 명령어도 적어준다

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    file = fopen(historyFile, "a+");
    if (strcmp(latest, cmdline))
        fprintf(file, "%s", cmdline); //가장 최근내역이랑 cmdline이랑 비교해서 다르면 적어준다
    fclose(file);

    if (check_history)
        printf("%s", cmdline);

    if (argv[0] == NULL)
        return;         /* Ignore empty lines */
    if (numofpipe == 0) // pipe가 없다는 의미 그냥 바로 실행
    {
        if (!builtin_command(argv))
        { // quit -> exit(0), & -> ignore, other -> run
            pid = Fork();
            if (pid == 0)
            {

                char temp[200] = "/bin/";
                char temp2[200] = "/usr/bin/";

                if (execve(argv[0], argv, environ) < 0&&execve(strcat(temp, argv[0]), argv, environ) < 0 && execve(strcat(temp2, argv[0]), argv, environ) < 0)
                { // ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
            /* Parent waits for foreground job to terminate */

            if (!bg)
            { //phase1과 같다
                int status;
                int temp;
                temp = waitpid(pid, &status, 0);
                if (temp < 0)
                    unix_error("waitfg: waitpid error");
                else
                    ;
            }
            else // when there is backgrount process!
                printf("%d %s", pid, cmdline);
        }

        return;
    }
    else // exec pipe
    {
        int **fd;
        fd = (int **)malloc(sizeof(int *) * (numofpipe));
        for (int i = 0; i < numofpipe; i++)
        {
            fd[i] = (int *)malloc(sizeof(int) * 2);
            if (pipe(fd[i]) < 0)
            {
                printf("pipe is wrong\n");
            }
        }
        for (int i = 0; i < numofpipe + 1; i++)
        { //pipe 개수 +1 만큼 반복문을 돌며 command 수행하도록 한다
            int temp_index = i / 2; //0,1 1,2 이렇게  pipe가 연결되도록 해준다
            bg = parseline(pipe_command[i], argv);
            if (!builtin_command(argv))
            { // quit -> exit(0), & -> ignore, other -> run
                pid = Fork();
                if (pid == 0)
                {
                    if (i == 0)
                    {
                        dup2(fd[temp_index][1], 1); //stdout 에 일단 복사
                        close(fd[temp_index][0]); // read, 0을 닫고
                    }
                    else if (i == numofpipe)
                    {
                        dup2(fd[temp_index][0], 0); //read를 stdin에
                        close(fd[temp_index][1]); //write 닫음
                    }
                    else
                    {
                        dup2(fd[temp_index][0], 0); //stdin에 read 넣고
                        close(fd[temp_index][1]); // write 파이프 닫음
                        dup2(fd[temp_index + 1][1], 1); //write 파이프를 stdout에 복사
                        close(fd[temp_index + 1][0]); //하나 다음거인 read pipe를 닫음
                    }

                    char temp[200] = "/bin/";
                    char temp2[200] = "/usr/bin/";

                    if (execve(argv[0], argv, environ) < 0&&execve(strcat(temp, argv[0]), argv, environ) < 0 && execve(strcat(temp2, argv[0]), argv, environ) < 0)
                    { // ex) /bin/ls ls -al &
                        printf("%s: Command not found.\n", argv[0]);
                        exit(0);
                    }
                }
                /* Parent waits for foreground job to terminate */

                if (!bg)
                {
                    close(fd[temp_index][1]); //부모일때 bg가 아니면 write을 닫아준다
                    int status;
                    int temp;
                    temp = waitpid(pid, &status, 0); //foreground가 끝날때 까지 대기
                    if (temp < 0)
                        unix_error("waitfg: waitpid error");
                    else
                        ;
                }
                else // when there is backgrount process!
                    printf("%d %s", pid, cmdline);
            }
        }
    }

    return;
}

/* If first arg is a builtin command, run it and return true */

int builtin_command(char **argv)
{

    if (!strcmp(argv[0], "cd"))//builtin command cd 면 return 1로 보내줘 /bin/cd 실행하도록
    {
        for (int i = 1; i; i++)
        {
            if (argv[i] == NULL)
            {
                break;
            }
            if (chdir(argv[i]))
            {
                printf("there is no directory: %s\n", argv[i]);
                break;
            }
        }
        return 1;
    }

    if (!strcmp(argv[0], "exit"))//종료 cmdline
    {
        exit(0);
    }

    if (!strcmp(argv[0], "history"))//buildin_command 로 history에 대한 명령어 수행
    {
        file = fopen(historyFile, "r");
        char history[200];
        int count = 1;
        while (EOF != fscanf(file, "%[^\n]\n", history))
        {
            printf("%d  %s\n", count++, history);
        }
        fclose(file);
        return 1;
    }

    if (!strcmp(argv[0], "quit")) /* quit command */
        exit(0);
    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;
    return 0; /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */

int parseline(char *buf, char **argv)
{
    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */
    int bg;      /* Background job? */



    char *point1 = buf;  // 포인터로 Buf를 가르키게 한다
    char *point2 = buf;  // 포인터로 Buf를 가르키게 한다
    
    while (*point1) {
        if (*point1 != '\''&&*point1!='\"') {
            // point 를 하나씩 이동하면서 따옴표가없다면
            // point2 에 point1 을 넣고 하나를 이동시킨다
            *point2++ = *point1;
        }
        point1++; //한칸씩 이동한다
    }
    
    // 가장 마지막에 널 문자를 넣는다
    *point2 = '\0';
    



 
    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

      /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) // Ignore spaces 
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
