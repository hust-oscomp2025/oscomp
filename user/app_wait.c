/*
 * This app fork a child process, and the child process fork a grandchild process.
 * every process waits for its own child exit then prints.                     
 * Three processes also write their own global variables "flag"
 * to different values.
 */

#include "user/user_lib.h"
#include "util/types.h"

int flag;
int main(void) {
    //test_kernel();
    
        flag = 0;
    int pid = fork();
		printu("forked pid=%d\n",pid);
    if (pid == 0) {
				printu("Child process continue\n");
        flag = 1;
				printu("Child process continue\n");

        pid = fork();
				printu("Child process continue\n");

				printu("forked pid=%d\n",pid);

        if (pid == 0) {
            flag = 2;
            printu("Grandchild process end, flag = %d.\n", flag);
        } else {
            wait(pid);
            printu("Child process end, flag = %d.\n", flag);
        }
    } else {
			printu("pid=%d\n",pid);
        wait(-1);
        printu("Parent process end, flag = %d.\n", flag);
    }
    
    

    exit(0);
    return 0;
}
