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
char history2[10] = "/history"; //경로 찾기
char historyFile[200];
int main()
{
    getcwd(historyFile, 200); //main 함수가 실행되는 현재 디렉토리 경로를 찾기
    strcat(historyFile, history2); //history root 
    char cmdline[MAXLINE]; /* Command line */
    while (1)
    {
        /* Read */
        printf("CSE4100-MP-P1> ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin))
            exit(0);

        /* Evaluate */

        eval(cmdline); //여기서 cmdline 에 대한 실행
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

    int check_history = 0; //history command 에 의한 명령어인지 아닌지 판단
    file = fopen(historyFile, "a+"); //history 파일 open
    late = 0;
    while (EOF != fscanf(file, "%[^\n]\n", latest))
    {
        late++;
    }
    strcat(latest, "\n"); //가장 최근 명령어 저장
    fclose(file);

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    if (argv[0] == NULL)
        return; /* Ignore empty lines */

    for (int i = 0; i > -1; i++) //history, ! 에 대한 parsing 작업
    {

        if (i == strlen(cmdline) - 1)
            break;
        if (cmdline[i] == '!' && cmdline[i + 1] == '!')
        {
            check_history = 1;
            if (late == 0)
            {
                printf("No history.\n"); //가장 최근 history 정보가 없을때
                return;
            }
            file = fopen(historyFile, "a+");
            char temp[200];
            while (EOF != fscanf(file, "%[^\n]\n", temp))
            {
            }
            fclose(file);
            char str[200] = {};
            strncpy(str, cmdline, i);

            strcat(str, temp);
            strcat(str, cmdline + i + 2);
            strcpy(cmdline, str); //cmdline을 가장 최근 history로 바꿔준다
            i = 0;
        }
        if (cmdline[i] == '!' && cmdline[i + 1] >= 48 && cmdline[i + 1] <= 57)
        { //!# 이 왔을 때 
            check_history = 1;
            int temp_index = i + 1;
            char temp_history[200];
            int num = 0;
            while (cmdline[temp_index] >= 48 && cmdline[temp_index] <= 57)
            { //숫자들만 인식하도록 해준다

                temp_history[num] = cmdline[temp_index];
                temp_index++;
                num++;
            }

            if (late < (atoi(temp_history)))
            {
                printf("Invalid command number.\n");
                return;
            } //숫자가 history보다 크면 오류

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

            strcpy(cmdline, str); //cmdline 를 다시 정리
            i = 0;
        }
    }

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); //따옴표 parsing 추가

    file = fopen(historyFile, "a+");
    if (strcmp(latest, cmdline))
        fprintf(file, "%s", cmdline);
    fclose(file);

    if (check_history)
        printf("%s", cmdline);

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
        {
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

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{

    if (!strcmp(argv[0], "cd")) //builtin command cd 면 return 1로 보내줘 /bin/cd 실행하도록
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

    if (!strcmp(argv[0], "exit")) //종료 cmdline
    {
        exit(0);
    }

    if (!strcmp(argv[0], "history")) //buildin_command 로 history에 대한 명령어 수행
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
