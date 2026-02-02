#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *a) {

    //prints usage and exits
    // It's called when the user provides invalid argumetns
    
    fprintf(stderr, "Usage: %s <pid>\n", a);
    exit(1);
}

static int isnumeric(const char *s) {
    
    // returns 1if the string consists only of digits
    // It's used to validate that the PID argument is numeric

    if (!s || !*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static void friendly_open_error(const char *path) {
    // outputs more friendly user error messages when opening / proc files fails
    // Shows the difference between missing PIDs and permission errors

    if (errno == ENOENT) DIE_MSG("PID not found");
    if (errno == EACCES) DIE_MSG("Permission denied");
    DIE(path);


}

int main(int c, char **v) {
    // expect exactly one argument: a numeric PID
    if (c != 2 || !isnumeric(v[1])) usage(v[0]);

    const char *pidstr = v[1];
    char path[256];

    // 1) Read /proc/<pid>/stat for state, PPID, utime, stime, and processor
    
    snprintf(path, sizeof(path), "/proc/%s/stat", pidstr);
    FILE *fs = fopen(path, "r");
    if (!fs) friendly_open_error(path);

    char buf[8192];
    if (!fgets(buf, sizeof(buf), fs)) {
        fclose(fs);
        DIE("fgets(stat)");
    }
    fclose(fs);

    char *rparen = strrchr(buf, ')');
    if (!rparen) DIE_MSG("Malformed /proc/<pid>/stat");
     // Save comm as a fallback if /proc/<pid>/cmdline is empty.
    char comm[256] = {0};
    {
        char *lparen = strchr(buf, '(');
        if (lparen && rparen > lparen) {
            size_t n = (size_t)(rparen - lparen - 1);
            if (n >= sizeof(comm)) n = sizeof(comm) - 1;
            memcpy(comm, lparen + 1, n);
            comm[n] = '\0';
        } else {
            strncpy(comm, "?", sizeof(comm) - 1);
        }
    }

    char *p = rparen + 2; 
    char *save = NULL;

    char state = '?';
    long ppid = -1;
    long usere_ticks = -1;
    long kernel_ticks = -1;
    long processor = -1;

   
    for (int tok = 1; ; tok++) {
        char *t = strtok_r(tok == 1 ? p : NULL, " ", &save);
        if (!t) break;

        if (tok == 1) state = t[0];             
        else if (tok == 2) ppid = strtol(t, NULL, 10); // field 3 
        else if (tok == 12) usere_ticks = strtol(t, NULL, 10); // field 4 
        else if (tok == 13) kernel_ticks = strtol(t, NULL, 10); // field 15
        else if (tok == 37) processor = strtol(t, NULL, 10);  // field 39
    }

    if (ppid < 0 || usere_ticks < 0 || kernel_ticks < 0 || processor < 0) {
        DIE_MSG("Failed to parse /proc/<pid>/stat");
    }

    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) DIE_MSG("sysconf(_SC_CLK_TCK) failed");

    double cpu_seconds = (double)(usere_ticks + kernel_ticks) / (double)hz;
     // 2) Read /proc/<pid>/cmdline for the full command line
    char cmdline[4096] = {0};
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pidstr);
    FILE *fc = fopen(path, "r");
    if (!fc) {
    
        if (errno == ENOENT) DIE_MSG("PID not found");
        if (errno == EACCES) DIE_MSG("Permission denied");
        DIE(path);
    }

    size_t nread = fread(cmdline, 1, sizeof(cmdline) - 1, fc);
    fclose(fc);

    if (nread > 0) {
        for (size_t i = 0; i + 1 < nread; i++) {
            if (cmdline[i] == '\0') cmdline[i] = ' ';
        }
        cmdline[nread] = '\0';
    } else {
    
        snprintf(cmdline, sizeof(cmdline), "%s", comm);
    }
     // 3) Read /proc/<pid>/status and extract VmRSS (resident memory in kB)
    long vmrss_kb = -1;
    snprintf(path, sizeof(path), "/proc/%s/status", pidstr);
    FILE *fst = fopen(path, "r");
    if (!fst) friendly_open_error(path);

    while (fgets(buf, sizeof(buf), fst)) {
        if (strncmp(buf, "VmRSS:", 6) == 0) {
            // Format: VmRSS:   4096 kB
            char label[64], unit[64];
            long val = -1;
            if (sscanf(buf, "%63s %ld %63s", label, &val, unit) >= 2) {
                vmrss_kb = val;
            }
            break;
        }
    }
    fclose(fst);

    if (vmrss_kb < 0) {
       // If VmRSS is missing, default to 0 instead of failing.
        vmrss_kb = 0;
    }
    // printing output in the format requested
    printf("PID:%s\n", pidstr);
    printf("State:%c\n", state);
    printf("PPID:%ld\n", ppid);
    printf("Cmd:%s\n", cmdline);
    printf("CPU:%ld %.3f\n", processor, cpu_seconds);
    printf("VmRSS:%ld\n", vmrss_kb);

    return 0;
}
