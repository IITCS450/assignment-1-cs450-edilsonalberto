#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define _POSIX_C_SOURCE 200809L

static void usage(const char *a){fprintf(stderr,"Usage: %s <cmd> [args]\n",a); exit(1);}
static double d(struct timespec a, struct timespec b){
 return (b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9;}
int main(int c,char**v){
/* TODO : ADD CODE HERE
*/
    if (c < 2){
        usage(v[0]);
    }

    struct timespec t0, t1;

    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0){
        DIE("clock_gettime(start)");
    }

    pid_t pid = fork();
    if (pid < 0){
        DIE("fork");
    }

    if (pid == 0) {

        execvp(v[1], &v[1]);
        perror("execvp");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        DIE("waitpid");
    }

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        DIE("clock_gettime(end)");
    }

    double elapsed = d(t0, t1);

    // Print required info: child PID, exit code OR signal, and elapsed time
    if (WIFEXITED(status)) {
        printf("pid=%d exit=%d time=%.6f\n", pid, WEXITSTATUS(status), elapsed);
    } else if (WIFSIGNALED(status)) {
        printf("pid=%d signal=%d time=%.6f\n", pid, WTERMSIG(status), elapsed);
    } else {
        // Rare cases (stopped/continued) — still print something sane
        printf("pid=%d status=%d time=%.6f\n", pid, status, elapsed);
    }

 return 0;
}
