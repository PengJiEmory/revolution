// THIS CODE IS MY OWN WORK, IT WAS WRITTEN WITHOUT CONSULTING
// A TUTOR OR CODE WRITTEN BY OTHER STUDENTS - PENG JI

/* CS551 Assignment 3 */
/* compute.c          */
/* Peng Ji            */

#include "perfect.h"

long testing;  //last tested
XDR handle_w;
int myid = 0;  //id registered in manage
/*
void terminate(int childid) {
    char end = 'e';
    kill(childid, SIGTERM);
    xdr_char(&handle_w, &end);
    xdr_int(&handle_w, &myid);
    xdr_long(&handle_w, &testing);
    exit(0);
}
*/
void terminate() {
    char end = 'e';
    xdr_char(&handle_w, &end);
    xdr_int(&handle_w, &myid);
    xdr_long(&handle_w, &testing);
    exit(0);
}

int main (int argc, char *argv[]) {
    if (argc != 4 || atoi(argv[3]) <= 0) {
        printf("Usage: compute server_name server_port start_number (> 0)\n");
        exit(1);
    }

    int skfd, newfd;;
    //int pid;
    int process_begin;
    long perf, i;
    long address, start_asked;
    long start, end;
    double timecost;
    char sndinit, rcvinit, perf_found;
    time_t time_start, time_end;
    sigset_t mask;
    FILE * stream;
    XDR handle_r;
    struct sigaction action;
    struct sockaddr_in sin;

    // install signal handler for INTR, QUIT and HANGUP
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    sigemptyset(&mask);

    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);

    action.sa_flags = 0;
    action.sa_mask = mask;
    action.sa_handler = terminate;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    address = *(long *) gethostbyname(argv[1])->h_addr;
    sin.sin_addr.s_addr = address;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(atoi(argv[2]));


    while(1) {
        // creat the socket
        if ((skfd =socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket");
            exit(1);
        }
        // try to connect to manage
        if (connect(skfd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
            close(skfd);
            continue;
        }
        break;
    }

    if ((stream = fdopen(skfd,  "r+")) == (FILE *) -1) {
        perror("fdopen");
        exit(1);
    }

    // attach XDR handle
    xdrstdio_create(&handle_w, stream, XDR_ENCODE);
    xdrstdio_create(&handle_r, stream, XDR_DECODE);

    sndinit = 'c';  // tell the manage this is compute

    start_asked = atol(argv[3]);
    timecost = 0;  // timecost which is expected as 15sec
    testing = 0;  // current testing number
    perf_found = 'p';  // tell the manage perfect number is found

    process_begin = 0;

    while(1) {
        // send the request
        xdr_char(&handle_w, &sndinit);
        xdr_int(&handle_w, &myid);
        xdr_long(&handle_w, &start_asked);
        xdr_double(&handle_w, &timecost);
        fflush(stream);
        // wait for response from manage
        xdr_char(&handle_r, &rcvinit);
        if (rcvinit == 't') terminate();
        else if (rcvinit == 'n') {
            xdr_long(&handle_r, &start);
            xdr_int(&handle_r, &myid);
            xdr_long(&handle_r, &end);
        }
        time_start = time(0);
        for (i = start; i <= end; i++) {
            if (isperf(i)) {
                // send perfect number
                xdr_char(&handle_w, &perf_found);
                xdr_int(&handle_w, &myid);
                xdr_long(&handle_w, &i);
                fflush(stream);
            }
            testing = i;
        }
        time_end = time(0);
        timecost = (double) (time_end - time_start);
    }
/*
    pid = fork();
    // child send data to manage
    if (pid == 0) {
        while(1) {
            process_begin = 0;
            // send the request
            xdr_char(&handle_w, &sndinit);
            xdr_int(&handle_w, &myid);
            xdr_long(&handle_w, &start_asked);
            xdr_double(&handle_w, &timecost);
            // wait for response from manage
            while(!process_begin)
            time_start = time(0);
            for (i = start; i <= end; i++) {
                if (isperf(i)) {
                    // send perfect number
                    xdr_char(&handle_w, &perf_found);
                    xdr_int(&handle_w, &myid);
                    xdr_long(&handle_w, &i);
                }
                testing = i;
            }
            time_end = time(0);
            timecost = (double) (time_end - time_start);
        }
        exit(0);
    }
    // parent receive data from manage
    else {
        while(1) {
            xdr_char(&handle_r, &rcvinit);
            if (rcvinit == 't') terminate(pid);
            else if (rcvinit == 'n') {
                xdr_long(&handle_r, &start);
                xdr_int(&handle_r, &myid);
                xdr_long(&handle_r, &end);
                // tell child to start computing
                process_begin = 1;
            }
        }
    }
*/
}

int isperf(long num) {
    long candidate = num;
    long i, sum = 0;
    for (i = 1; i <= candidate; i++) {
        if (candidate % i == 0) sum += i;
    }
    if (sum == candidate + candidate) return 1;
    return 0;
}
