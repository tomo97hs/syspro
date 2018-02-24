#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_ARGS 30
#define MAX_LEN  100

void do_child(char *argv[MAX_ARGS]);
void redirect_exec(int redirect, char *io, char *comm1[MAX_ARGS]);
void sigint_handler();

int main()
{
	int argc, n = 0;
  	int status;
	int cpid, cpid2; 
  	char input[MAX_LEN], *argv[MAX_ARGS], *cp;
  	const char *delim = " \t\n"; // コマンドの区切り文字
	struct sigaction sa_sigint;

	sa_sigint.sa_handler = sigint_handler;
	sa_sigint.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa_sigint, NULL) < 0) {
        perror("sigaction");
		exit(1); 
	}

	while (1) {
		/* プロンプトの表示 */
		printf("./myshell[%02d]> ", n++); 

		/* 1行読み込む */
		if (fgets(input, sizeof(input), stdin) == NULL)
		/* EOF(Ctrl-D)が入力された */
			exit(1);

		/* 空白,タブ区切りでコマンド列に分割する */
		cp = input;
		for (argc = 0; argc < MAX_ARGS; argc++) {
			if ((argv[argc] = strtok(cp, delim)) == NULL)
				break;
			cp = NULL;
		}

		char *comm1[MAX_ARGS], *comm2[MAX_ARGS], *comm3[MAX_ARGS];
		char *input, *output;
		int hasComm2 = 0, i = 0, j = 0, k = 0;
		int pipeOpe = 0, redirect = 0;
		
		/* 入力されたコマンドをcomm1,comm2,comm3に分割する */
		while (argv[i]) {
			if (hasComm2 == 0) {
				
				// ">"が入力された時
				if (*argv[i] == '>') {
					input = argv[i + 1];
					redirect = 1;
					redirect_exec(redirect, input, comm1);
					i += 2;
					if (argv[i] == NULL)
						break;
				}
				
				// "<"が入力された時
				if (*argv[i] == '<') {
					comm1[i] = argv[i + 1];
					output = argv[i + 1];
					redirect = 2;
					redirect_exec(redirect, output, comm1);
					i += 2;
					if (argv[i] == NULL)
						break;
				}
				
				// "|"が入力された時
				if (*argv[i] == '|') {
					hasComm2 = 1;
					pipeOpe = 1;
				} else if (*argv[i] == '?') {
			    		hasComm2 = 1;	
				} else {
					comm1[i] = argv[i];
			    		comm1[i + 1] = NULL;
				}
			} else if (hasComm2 == 1) {

				// ":"が入力された時
				if (*argv[i] == ':') {
					if (pipeOpe == 1) {
						printf("パイプと三項演算子は同時に指定できません\n");
						exit(1);
					} 
					hasComm2 = 2;
				} else {
					comm2[j] = argv[i];
					comm2[j + 1] = NULL;
					j++;
				}		
			} else {
				comm3[k] = argv[i];
				comm3[k + 1] = NULL;
				k++; 
			}
			if (redirect == 0)
		    		i++;
		}
		
		/* byeが入力されると終了 */
		if (strcmp(argv[0], "bye") == 0)
			exit(1);

		/* "|"が入力された時にパイプを作成 */
		int pipe_fds[2];
        	if (pipeOpe == 1) {
			if (pipe(pipe_fds) < 0) {
				perror("pipe");
				exit(1);
			}
		}
		if (redirect == 0) {
			cpid = fork();
			if (cpid < 0) {
				// fork()に失敗した
				perror("fork");
				exit(1);
			}
			
			if (cpid == 0) {
				if (pipeOpe == 0) {
					/* 子1プロセス時に実行 */
					do_child(comm1);
				} else {
					/* "|"を含んでおり子1プロセス時に実行 */
					close(pipe_fds[0]);
					close(1);
					dup2(pipe_fds[1], 1);
					close(pipe_fds[1]);
					do_child(comm1);
				}
			} else if (pipeOpe == 1) {
				/* 子1プロセスの終了を待つ */
				if (wait(&status) < 0) {
					perror("wait");
					exit(1);
				}
				close(pipe_fds[1]);
				cpid2 = fork();

				/* 子2プロセス時に実行 */
				if (cpid2 == 0) {
					close(pipe_fds[1]);
					close(0);
					dup2(pipe_fds[0], 0);
					close(pipe_fds[0]);
					do_child(comm2);
				}
				close(pipe_fds[0]);
			} else {
				/* 親プロセスの処理 */
				if (wait(&status) < 0) {
					perror("wait");
					exit(1);
				}
				if (hasComm2 == 2) {
					cpid2 = fork();
					if (cpid2 == 0 && status == 0)
						/* 子2プロセス時に実行 */
						do_child(comm2);
					if (cpid2 == 0 && status != 0)
						do_child(comm3);

					/* 子2プロセスの終了を待つ */
					if (wait(&status) < 0) {
						perror("wait");
						exit(1);
					}
				}
			}
			if (pipeOpe == 1) {
				/* 子2プロセスの終了を待つ */
				if (wait(&status) < 0) {
					perror("wait");
					exit(1);
				}
			}
			if (status != 0 && status != 2)
				printf("Process %d existed with status(%d).\n", cpid, WEXITSTATUS(status));
		}
	}
}


void do_child(char *argv[MAX_ARGS]) 
{
	int ret; 

	ret = execvp(argv[0], argv);
	/* 制御が戻ってくるのは、エラーが発生した場合のみ */
	perror(argv[0]);
	exit(ret);
}

void redirect_exec(int redirect, char *io, char *comm1[MAX_ARGS])
{
	int cpid, fd, status = 0;
	
	cpid = fork();
	if (cpid < 0) {
		// fork()に失敗した
		perror("fork");
		exit(1);
	}

	/* 子プロセスの処理 */
	if (cpid == 0){
		if (redirect == 0) {
			/* 子1プロセス時に実行 */
			do_child(comm1);
		} else if (redirect == 1) {
			/* ">"を含んでおり子1プロセス時に実行 */
			fd = open(io, O_CREAT | O_WRONLY, 0666);
				if (fd < 0) {
					perror("open1"); 
					close(fd); 
					exit(1);
				}
			close(1);
			if (dup2(fd, 1) < 0) {
				perror("dup2");
				close(fd);
				exit(1);
			}
			close(fd);
			do_child(comm1);
		} else if (redirect == 2) {
			/* "<"を含んでおり子1プロセス時に実行 */
			fd = open(io, O_RDONLY);
			if (fd < 0) {
				perror("open"); 
				close(fd); 
				exit(1);
			}
			close(0);
			if (dup2(fd, 0) < 0) {
				perror("dup2");
				close(fd);
				exit(1);
			}
			close(fd);
			do_child(comm1);
		}
	} else {
		/* 親プロセスの処理 */
		if (wait(&status) < 0) {
			perror("wait");
			exit(1);
		}
	}
	if (status != 0)
		printf("Process %d existed with status(%d).\n", cpid, WEXITSTATUS(status));
}

void sigint_handler()
{
	write(1,  " (SIGINT caught!)\n", 18);
}