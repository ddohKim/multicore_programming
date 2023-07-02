Phase1.
./myshell 을 입력하면 myshell이 실행된다.
여기서는 기본 명령어인 cd, ls, mkdir, rmdir, touch, cat, echo, history, exit 등의 명령어가 실행 되어야 한다. 
Built-in command가 아닌 명령어를 fork 함수와 execve 함수를 통해 구현하고 built-in command 는 직접 구현을 해주었다.
History.txt는 가장 처음 Main 함수에서 root 가 잡히는데 이는 어느 환경에서나 돌아갈 수 있도록 해당 바이너리파일이 실행되는 곳에 history가 생성되어 어느 경로를 타고 들어가도 history 구현에 문제가 없다.
추가적으로 parsing 같은 경우도 따옴표 파싱을 추가해서 따옴표를 무시하고 명령어가 수행할 수 있다.
---------------
Phase2.
여기는 추가적으로 파이프 명령어를 수행 할 수 있도록 하였다.
pipe parsing 을 통해 명령어들을 따로 나누어 배열에 저장하고 파이프 기준 왼쪽에 해당하는 명령어의 출력이 파이프 오른쪽의 입력이 되도록 구현하였다.
여기서는 dup2(a,b) 함수로 a를 b에 복사해주고 close 함수로 해당 fd를 닫아주면서 진행했다. 마지막 결과값을 stdout 하면 된다. 

자세한 구현 방법은 모두 document에 설명하였다. 