// THIS CODE IS MY OWN WORK, IT WAS WRITTEN WITHOUT CONSULTING
// A TUTOR OR CODE WRITTEN BY OTHER STUDENTS - PENG JI

/* CS551 Assignment 5 */
/* report.h           */
/* Peng Ji            */

#include "perfect.h"

int main(int argc, char *argv[]) {
    struct sockaddr_in sin;
    long address;
    int skfd;
    char sndinit, rcvinit, perf_end, msg_end;
    FILE * stream;
    XDR handle_r, handle_w;
    long perf;
    int compute_id;
    char * compute_host;
    char * perf_host;
    long compute_tested;
    long compute_start;
    long compute_end;
    char compute_stat;

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

    // send the request
    if ((stream = fdopen(skfd, "r+")) == (FILE *) -1) {
        perror("fdopen");
        exit(1);
    }

    xdrstdio_create(&handle_r, stream, XDR_DECODE);
    xdrstdio_create(&handle_w, stream, XDR_ENCODE);

    // tell the manage this is report
    sndinit = 'r';
    if (argc == 4) {
        if (argv[3][0] == '-' && argv[3][1] == 'k') {
            sndinit = 't';
        }
    }
    xdr_char(&handle_w, &sndinit);

    // receive messages
    printf("receiving from manage...\n");
    xdr_char(&handle_r, &rcvinit);
    if (rcvinit == 'o') {
        printf("no perfect number has been found yet\n");
    }

    else {
        printf("receiving perfect number records\n");
        perf_end = 0;
        while (!perf_end) {
            xdr_char(&handle_r, &perf_end);
            if (!perf_end) {
                xdr_long(&handle_r, &perf);
                xdr_string(&handle_r, &perf_host, SIZELIMIT);
                printf("perfect number %ld\t was found by %s\n", perf, perf_host);
            }
        }
    }

    xdr_char(&handle_r, &rcvinit);
    if (rcvinit == 'o') {
        printf("no compute has been registed yet\n");
        exit(0);
    }

    printf("receiving compute process records\n");
    msg_end = 0;
    while (!msg_end) {
        xdr_char(&handle_r, &msg_end);
        if (!msg_end) {
            xdr_int(&handle_r, &compute_id);
            xdr_long(&handle_r, &compute_tested);
            xdr_string(&handle_r, &compute_host, SIZELIMIT);
            xdr_long(&handle_r, &compute_start);
            xdr_char(&handle_r, &compute_stat);
            xdr_long(&handle_r, &compute_end);
            printf("compute %d on %s has tested %ld numbers.\nIts latest range is %ld - %ld with termination status as %c\n", compute_id, compute_host, compute_tested, compute_start, compute_end, compute_stat);
        }
    }
    fflush(stream);
    //close(skfd);
    return 0;
}
