/* $begin shellmain */
#include "myshell.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXARGS 200

#define zero 0
#define r 1 // background run
#define d 3 // done
#define s 4 // stop
#define k 5 // kill
#define f 2 // foreground run

typedef struct
{
    int job_id;
    int temp;
    int status;
    pid_t pid;
    char commandline[8200];
} job; // job에 대한 정보들을 저장하는 구조체

job jobs[200];   // 해당 구조체를 배열로 만들어 저장한다
int job_num_index = 0; // job 이 어디까지 진행되는지에 대한 index 변수

FILE *file;

/* Function prototypes */
void eval(char *commandline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
char latest[200];
int late = 0;
char history2[10] = "/history"; // history 저장하는 root
char historyFile[200];

void sigchld_handler(int sig)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WCONTINUED)) > 0)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status))
            if (pid >= 1) // pid가 1보다 크다
               { for (int i = 0; i < 200; i++)
                    if ((jobs[i].pid == pid))
                    {
                        int temp = jobs[i].status; // status가 zero,kill 가 아닐때는 해당 job을 done으로 바꿔준다(끝내준다)
                        if (temp == 1 || temp == 2 || temp == 3 || temp == 4)
                        {
                            jobs[i].status = d;
                            break;
                        }
                    }
               }
    }
    return;
}

void sigtstp_handler(int sig)
{
    int i = 0;
    while (1)
    {
        if (i == 200)
            break;
        if (jobs[i].status != zero&&jobs[i].status == f)
        {
            if (kill(-jobs[i].pid, SIGSTOP) < -1)
            {
                printf("kill error\n");
            }
            jobs[i].status = s; // 해당 job의 상태를 stop으로 바꾼다
            printf("[%d] Suspended\t%s", jobs[i].job_id, jobs[i].commandline);
            job_num_index++;
            return;
        }
        i++;
    }
    return;
}

void sigint_handler(int sig)
{
    // printf("SIGINT signal received.\n");
    int i = 0;
    while (1)
    {
        if (i == 200)
            break;
        if (jobs[i].status != zero&&jobs[i].status ==f)
        {
            if (kill(jobs[i].pid, SIGINT) < -1)
            {
                printf("kill error\n");
            }
            jobs[i].status = zero; // 강제 종료 되었기 때문에 zero로 바꾼다
            // printf("[%d] Stopped\t\t%s", jobs[i].job_id, jobs[i].commandline);
            job_num_index++;
            return;
        }
        i++;
    }
}

void job_ended(job *jobs)
{
    int i = 0;
    int max = 0;
    for (i = 0; i < job_num_index; i++)
    { // 만약 job의 상태가 kill 이나 done 일경우는 zero 로 바꾸고 done은 해당 정보를 출력하도록 해준다
        if (jobs[i].status == k)
        {
            jobs[i].status = zero;
            // printf("[%d] Terminated\t%s", jobs[i].job_id, jobs[i].commandline);
        }
        if (jobs[i].status == d)
        {
            jobs[i].status = zero;
            printf("[%d] Done\t%s", jobs[i].job_id, jobs[i].commandline);
        }
        if (jobs[i].status == zero || jobs[i].status == k)
        {
            max = 1;
        }
        else
        {
            max = 0;
        }
    }

    for (int i = job_num_index; i >= 0; i--) // 반대로 job에 대해 탐색해서 running 이나 stop을 탐색한다
    {
        if ((jobs[i].status == r) || (jobs[i].status == s))
        {
            job_num_index = i + 1; // job_num을 1 증가시킨다
            break;
        }
    }
    if (max == 1 && job_num_index >= 200)
    {
        job_num_index = 0;
    }
}

int main()
{
    getcwd(historyFile, 200); // 현재 main이 실행되는 directory root 에서 history를 가져온다
    strcat(historyFile, history2);

    for (int i = 0; i < 200; i++)
    { // job을 초기화 시켜주다
        jobs[i].pid = 0;
        jobs[i].job_id = 0;
        jobs[i].status = zero;
        jobs[i].commandline[0] = '\0';
    }
    char commandline[MAXLINE]; /* Command line */
    while (1)
    {

        signal(SIGCHLD, sigchld_handler); // signal 들을 받을 수 있도록 준비를 해준다
        signal(SIGTSTP, sigtstp_handler);
        signal(SIGINT, sigint_handler);

        /* Read */
        printf("CSE4100-MP-P1> ");
        fgets(commandline, MAXLINE, stdin);

        if (feof(stdin))
            exit(0);

        /* Evaluate */

        eval(commandline);
        job_ended(jobs); // eval 에서 모든 명령어를 수행한 후 마무리 체크를 하러 들어옴
    }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */

void eval(char *commandline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process job_id */

    file = fopen(historyFile, "a+");
    late = 0;
    while (EOF != fscanf(file, "%[^\n]\n", latest))
    {
        late++;
    }
    strcat(latest, "\n"); // 가장 최근 History 내역을 가져온다
    fclose(file);

    int check_history = 0;
    for (int i = 0; i > -1; i++)
    { //! 가 있는지 없는지  탐색

        if (i == strlen(commandline) - 1)
            break;
        if (commandline[i] == '!' && commandline[i + 1] == '!')
        {
            check_history = 1; // if 문안에 들어왔기 때문에 있다고 판단
            if (late == 0)
            { // 가장 최근 내역이 없을 때
                printf("No history.\n");
                return;
            }
            file = fopen(historyFile, "a+");
            char temp[200];
            while (EOF != fscanf(file, "%[^\n]\n", temp))
            {
            } // 가장 최근 내역을 cmdline으로 바꾼다
            fclose(file);
            char str[200] = {};
            strncpy(str, commandline, i);

            strcat(str, temp);
            strcat(str, commandline + i + 2);
            strcpy(commandline, str);
            i = 0;
        }
        if (commandline[i] == '!' && commandline[i + 1] >= 48 && commandline[i + 1] <= 57)
        {
            check_history = 1;
            int temp_index = i + 1;
            char temp_history[200];
            int num = 0;
            while (commandline[temp_index] >= 48 && commandline[temp_index] <= 57)
            {

                temp_history[num] = commandline[temp_index];
                temp_index++;
                num++;
            }

            if (late < (atoi(temp_history)))
            { //! # 이 history 숫자보다 클때
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

            strncpy(str, commandline, i);

            strcat(str, history);
            strcat(str, commandline + i + num + 1);

            strcpy(commandline, str); // cmdline을 바꿔준다
            i = 0;
        }
    }

    strcpy(buf, commandline);
    char pipe_command[200][200] = {};

    int numofpipe = 0;

    for (int i = 0; i < strlen(commandline); i++) // pipe 개수 찾아보기
    {
        if (commandline[i] == '|')
        {
            numofpipe++;
        }
    }
    for (int i = 0; i < numofpipe; i++) // pipe parsing
    {
        strncpy(pipe_command[i], buf, strchr(buf, '|') - buf);
        strcat(pipe_command[i], " ");
        strcpy(buf, strchr(buf, '|') + 1); // buf에 pipe parsing 한 정보들을 적어준다
    }
    strcat(pipe_command[numofpipe], buf); // 마지막 명령어도 적어준다

    file = fopen(historyFile, "a+");
    if (strcmp(latest, commandline))
        fprintf(file, "%s", commandline); // 가장 최근내역이랑 cmdline이랑 비교해서 다르면 적어준다
    fclose(file);

    if (check_history)
        printf("%s", commandline);

    int isBg = strlen(commandline);
    bg = 0;
    while (isBg)
    {
        if (commandline[isBg] == '&')
        {
            bg = 1;
            commandline[isBg] = ' ';
            break;
        }
        isBg--;
    }
    strcpy(buf, commandline);
    parseline(buf, argv);

    if (argv[0] == NULL)
        return;         /* Ignore empty lines */
    if (numofpipe == 0) // pipe가 없다는 의미 그냥 바로 실행
    {
        if (!builtin_command(argv))
        { // quit -> exit(0), & -> ignore, other -> run
            pid = Fork();
            if (pid == 0)
            {

                if (!bg)
                {
                    signal(SIGINT, sigint_handler); // foreground 에서는 crtrl+z, crtl+c가 작동되도록 signal을 열어준다.
                    signal(SIGTSTP, sigtstp_handler);
                }
                else
                {
                     signal(SIGINT, SIG_IGN); // background 에서는 crtrl+z, crtl+c 해당 signal을 막는다.
                    signal(SIGTSTP, SIG_IGN);
                }
                char temp[200] = "/bin/";
                char temp2[200] = "/usr/bin/";

                if (execve(argv[0], argv, environ) < 0 && execve(strcat(temp, argv[0]), argv, environ) < 0 && execve(strcat(temp2, argv[0]), argv, environ) < 0)
                { // ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[0]);
                    return;
                }
            }
            /* Parent waits for foreground job to terminate */

            else
            {

                if (!bg)
                { 

                    int status;
                    int temp;

                    jobs[job_num_index].job_id = job_num_index + 1; // 예를들어 첫번째 job의 id는 1이다
                    strcpy(jobs[job_num_index].commandline, commandline);
                    jobs[job_num_index].status = f; // bg가 아니기 때문에 해당 상태를 foreground로 해주고
                    jobs[job_num_index].pid = pid;  // return 된 pid 값을 넣어준다

                    temp = waitpid(pid, &status, WUNTRACED); // ctrl+z 에 대해 wait 할수 있도록 해준다(WUNTRACED)
                    job_num_index++;
                    if (temp < 0)
                        unix_error("waitfg: waitpid error");
                    else
                    { // 기다린 이후 프로세스를 다시 초기화 시켜야 한다
                        for (int i = 0; i < 200; i++)
                        {
                            if (jobs[i].status == f)
                            {
                                // job을 초기화 시켜주다
                                jobs[i].pid = 0;
                                jobs[i].job_id = 0;
                                jobs[i].status = zero;
                                jobs[i].commandline[0] = '\0';
                            }
                        }
                    }
                    return;
                }
                else
                {                                       // when there is background process!
                    jobs[job_num_index].job_id = job_num_index + 1; // 예를들어 첫번째 job의 id는 1이다
                    strcpy(jobs[job_num_index].commandline, commandline);
                    jobs[job_num_index].status = r; // background에서 실행된다는 의미
                    jobs[job_num_index].pid = pid;

                    printf("[%d] %d\t%s", jobs[job_num_index].job_id, jobs[job_num_index].pid, jobs[job_num_index].commandline);
                    job_num_index++;
                    return;
                } // when there is backgrount process!
            }
        }
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
        {                           // pipe 개수 +1 만큼 반복문을 돌며 command 수행하도록 한다
            int temp_index = i / 2; // 0,1 1,2 이렇게  pipe가 연결되도록 해준다

            parseline(pipe_command[i], argv);


            if (!builtin_command(argv))
            { // quit -> exit(0), & -> ignore, other -> run
                pid = Fork();
                if (pid == 0)
                {
                    if (!bg)
                    {
                        signal(SIGINT, sigint_handler); // ctrl+z, ctrl+c 가 작동할 수 있도록 signal을 받는다
                        signal(SIGTSTP, sigtstp_handler);
                    }
                    else
                    {
                          signal(SIGINT, SIG_IGN); //signal을 block 한다. 여기서는 실행되면 안됨
                        signal(SIGTSTP, SIG_IGN);
                    }

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
                    if (execve(argv[0], argv, environ) < 0 && execve(strcat(temp, argv[0]), argv, environ) < 0 && execve(strcat(temp2, argv[0]), argv, environ) < 0)
                    { // ex) /bin/ls ls -al &
                        printf("%s: Command not found.\n", argv[0]);
                        return;
                    }
                }

                /* Parent waits for foreground job to terminate */
                else
                {
                    if (!bg)
                    {
                        
                        close(fd[temp_index][1]); // 부모일때 bg가 아니면 닫아준다

                        if (i == numofpipe)
                        {
                            if (jobs[job_num_index].status == zero)
                            {
                                jobs[job_num_index].job_id = job_num_index + 1; // job id 를 설정
                                strcpy(jobs[job_num_index].commandline, commandline);
                                jobs[job_num_index].status = f; // bg가 아니기 때문에 foreground에서 돌아간다                            jobs[job_num_index].pid = pid;

                                jobs[job_num_index].pid = pid;
                                // printf("[%d] %d\t%s", jobs[job_num_index].job_id, jobs[job_num_index].pid, jobs[job_num_index].commandline);
                                job_num_index++;
                            }

                            int status;
                            int temp;

                            temp = waitpid(pid, &status, WUNTRACED); // ctrl+z 에대해서 작동하도록 WUNTRACED 추가한다

                            if (temp < 0)
                                unix_error("waitfg: waitpid error");
                            else
                            {
                                for (int i = 0; i < MAXARGS; i++)
                                {
                                    if (jobs[i].status == f) // 해당 foregound에 대한 작업이 끝났으니 초기화를 시켜준다
                                    {
                                        // job을 초기화 시켜주다
                                        jobs[i].pid = 0;
                                        jobs[i].job_id = 0;
                                        jobs[i].status = zero;
                                        jobs[i].commandline[0] = '\0';
                                    }
                                }
                            }
                        }
                    }
                    else
                    {                             // background에서 시행될 때
                        close(fd[temp_index][1]); // 부모일때 bg가 아니면 닫아준다

                        if (i == numofpipe)
                        {

                            if (jobs[job_num_index].status == zero)
                            {
                                jobs[job_num_index].job_id = job_num_index + 1;
                                strcpy(jobs[job_num_index].commandline, commandline);
                                jobs[job_num_index].status = r; // background에서 실행됨을 의미
                                jobs[job_num_index].pid = pid;

                                printf("[%d] %d\t%s", jobs[job_num_index].job_id, jobs[job_num_index].pid, jobs[job_num_index].commandline);
                                job_num_index++;
                                return;
                            }
                        }
                    }
                }
            }
        }
        return;
    }

    return;
}

/* If first arg is a builtin command, run it and return true */

int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "jobs"))
    { //job 명령어를 실행시키면 job struct 구조체를 출력하도록 한다
        for (int i = 0; i < 200; i++)
        {
            if (jobs[i].status == r)
            {
                printf("[%d] Running\t%s", jobs[i].job_id, jobs[i].commandline);
            }
            if (jobs[i].status == s)
            {
                printf("[%d] Suspended\t%s", jobs[i].job_id, jobs[i].commandline);
            }
            if (jobs[i].status == d)
            {
                printf("[%d] Done\t%s", jobs[i].job_id, jobs[i].commandline);
            }
            if (jobs[i].status == k)
            {
                printf("[%d] Terminated\t%s", jobs[i].job_id, jobs[i].commandline);
            }
        }
        return 1;
    }

    if (!strcmp(argv[0], "kill"))
    { //kill 명령어가 들어오면 해당 job 을 sigkill signal을 보내 죽이도록 한다. 
        if (argv[1][0] != '%')
        {
            printf("command not found.\n");
            return 1;
        }

        int job_id = -1;
        argv[1][0] = ' ';
        job_id = atoi(argv[1]) - 1;

        if (job_id == -1 || jobs[job_id].status == zero)
        {
            printf("no such job\n");
            return 1;
        }
        jobs[job_id].status = k; //상태를 kill로 변경
        if (kill(jobs[job_id].pid, SIGKILL) < -1)
        {
            printf("kill error\n");
            return 1;
        }
    }

    if (!strcmp(argv[0], "bg")) //background에서 실행시킴
    {
        if (argv[1][0] != '%')
        {
            printf("command not found.\n");
            return 1;
        }
        int job_id = -1;
        argv[1] = argv[1] + 1;
        job_id = atoi(argv[1]) - 1;
        if (job_id == -1 || jobs[job_id].status == zero)
        {
            printf("no such job\n");
            return 1;
        }
        jobs[job_id].status = r; //running 이라는 의미로 background에서 실행

        if (kill(-jobs[job_id].pid, SIGCONT) < -1) //sigcont 시그널 보냄
        {
            printf("kill error\n");
            return 1;
        }

        if (jobs[job_id].status == r)
        { //bg 명령어 실행 시 다음과 같은 stdout 출력해야 한다
            printf("[%d] Running\t%s", jobs[job_id].job_id, jobs[job_id].commandline);
        }

        return 1;
    }

    if (!strcmp(argv[0], "fg"))
    { //fg 명령어 실행 시
        if (argv[1][0] != '%')
        {
            printf("command not found.\n");
            return 1;
        }
        int job_id = -1;
        argv[1][0] = ' ';
        job_id = atoi(argv[1]) - 1;
        if (job_id == -1 || jobs[job_id].status == zero)
        {
            printf("no such job\n");
            return 1;
        }

        jobs[job_id].status = f; //해당 job 을 foreground 로 바꾸고
        if (kill(-jobs[job_id].pid, SIGCONT) < -1)
        {
            printf("kill error\n");
            return 1;
        }
        if (jobs[job_id].status == f)
        { //출력
            printf("[%d] Running\t%s", jobs[job_id].job_id, jobs[job_id].commandline);
        }

        while (1)
        {

            if (jobs[job_id].status == f) // job의 상태가 foreground가 아닐때까지 수행
                {
                    sleep (1); //계속해서 돌지 않게 다른 일 하도록 sleep 해줌
                }
            else
                return 1;
        }
    }

    if (!strcmp(argv[0], "cd")) // cd built-in commandline 수행
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

    if (!strcmp(argv[0], "exit")) // exit command line 수행
    {
        exit(0);
    }

    if (!strcmp(argv[0], "history")) // history 수행
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
