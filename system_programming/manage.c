// THIS CODE IS MY OWN WORK, IT WAS WRITTEN WITHOUT CONSULTING
// A TUTOR OR CODE WRITTEN BY OTHER STUDENTS - PENG JI

/* CS551 Assignment 5 */
/* manage.c           */
/* Peng Ji            */

#include "perfect.h"

int terminating = 0;

void terminate() {
    // send terminate signal out
    terminating = 1;
    printf("terminating ...\n");
}

int main (int argc, char *argv[]) {
    if (argc != 2 || atoi(argv[1]) <=0 ) {
        printf("Usage: manage port_number\n");
        exit(1);
    }

    int skfd, newfd;
    int nfds, rv;
    int len, i;
    int compid;
    int comp_total = 0;
    int comp_active = 0;
    int waiting_terminate = 0;  // waiting report
    int disconnect = 0;
    long newperf;
    long start_req, start, end, last, newrange;
    double timecost;
    char buffer[32]; //find recv
    char sndinit, rcvinit, perf_end, msg_end;
    sigset_t mask;
    FILE * stream;
    struct sigaction action;
    struct sockaddr_in sin; // structure for socket address
    struct sockaddr_in tempsin;
    struct hostent * hostentp;
    struct pollfd *ufds = (struct pollfd *) calloc(sizeof(struct pollfd), CLTCOUNT);

    struct perfent {
        long value;
        int comid;
        struct perfent * next;
    };

    struct compent {
        int id; // assocaite with hostname
        int socketfd;
        char hostname[256];
        long tested;
        long start;
        long end;
        long last;
        char stat;  //'Y' or 'N'
    };

    struct range {
        long start;
        long end;
        struct range * prev;
        struct range * next;
    };

    //struct perfent * perf_head = (struct perfent *) malloc (sizeof (struct perfent));  //perfect number record
    struct perfent * perf_head = NULL;
    //if (perf_head == NULL) printf("I am right\n");
    //else printf("I am wrong\n");
    struct perfent * perf_curr;
    struct perfent * perf_temp;
    struct compent * complist[CLTCOUNT];  //compute record
    //struct range * range_head = (struct range *) malloc(sizeof(struct range));  //range record
    struct range * range_head = NULL;
    struct range * range_curr;
    struct range * range_temp;

    // install signal handler for INTR, QUIT and HANGUP
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    sigemptyset(&mask);

    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);

    action.sa_flags   = 0;
    action.sa_mask    = mask;
    action.sa_handler = terminate;

    sigaction(SIGINT,  &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGHUP,  &action, NULL);

    // set up IP addr and port number for bind
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(atoi(argv[1]));

    // get an internet socket for stream connections
    if ((skfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        exit(1);
    }

    // add the socket fd to ufds
    nfds = 1;
    // ufds[0] = (struct pollfd) malloc(sizeof(struct pollfd));
    ufds[0].fd = skfd;
    ufds[0].events = POLLIN | POLLHUP | POLLRDNORM;
    //ufds[0].events = POLLOUT;

    // do the actual bind
    if (bind (skfd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        perror("bind");
        exit(2);
    }

    // allow a connection queue for up to CLTCOUNT clients
    listen(skfd, CLTCOUNT);

    // Now loop accepting connections
    while (1) {
        // check if comp_active is 0
        if (terminating) {
            if (terminating == 1) {
                for (i = 0; i < comp_total; i++) {
                    FILE * stream_rt;
                    if ((stream_rt = fdopen(complist[i]->socketfd, "r+")) == (FILE *) -1) {
                        perror("fdopen_caseT");
                        exit(1);
                    }
                    rcvinit = 't';
                    XDR handle_wrt;
                    xdrstdio_create(&handle_wrt, stream_rt, XDR_ENCODE);
                    xdr_char(&handle_wrt, &rcvinit);
                    fflush(stream_rt);
                }
                terminating = 2;
            }
            if (comp_active == 0) {
            // check if there is waiting report
                if (waiting_terminate > 0) {
                    FILE * stream_t;
                    XDR handle_wt;
                    if ((stream_t = fdopen(waiting_terminate, "r+")) == (FILE *) -1) {
                        perror("fdopen");
                        exit(1);
                    }
                    xdrstdio_create(&handle_wt, stream_t, XDR_ENCODE);
                    // send perf info back
                    if (perf_head == NULL) {
                        printf("We have not found any perfect number yet\n");
                        sndinit = 'o';
                        xdr_char(&handle_wt, &sndinit);
                    }
                    else {
                        sndinit = 'p';
                        xdr_char(&handle_wt, &sndinit);
                        perf_end = 0;
                        perf_curr = perf_head;
                        while (perf_curr != NULL) {
                            xdr_char(&handle_wt, &perf_end);
                            xdr_long(&handle_wt, &(perf_curr->value));
                            xdr_string(&handle_wt, &(complist[perf_curr->comid - 1]->hostname), SIZELIMIT);
                            perf_curr = perf_curr->next;
                        }
                        perf_end = 1;
                        xdr_char(&handle_wt, &perf_end);
                    }
                    fflush(stream_t);
                    // send comp info back
                    if (comp_total == 0) {
                        printf("We have no compute registered yet\n");
                        sndinit = 'o';
                        xdr_char(&handle_wt, &sndinit);
                        fflush(stream_t);
                        break;
                    }
                    sndinit = 'c';
                    xdr_char(&handle_wt, &sndinit);
                    msg_end = 0;
                    for (i = 0; i < comp_total; i++) {
                        xdr_char(&handle_wt, &msg_end);
                        xdr_int(&handle_wt, &(complist[i]->id));
                        xdr_long(&handle_wt, &(complist[i]->tested));
                        xdr_string(&handle_wt, &(complist[i]->hostname), SIZELIMIT);
                        xdr_long(&handle_wt, &(complist[i]->start));
                        xdr_char(&handle_wt, &(complist[i]->stat));
                        xdr_long(&handle_wt, &(complist[i]->end));
                    }
                    msg_end = 1;
                    xdr_char(&handle_wt, &msg_end);
                    fflush(stream_t);
                }
                // terminate itself
                exit(0);
            }
        }
        int newclients = 0;
        rv = poll(ufds, nfds, 0);
        if (rv == -1) {
            perror("poll");
            exit(0);
        }
        if (rv == disconnect) continue;
        for (i = 0; i < nfds; i++) {
            disconnect = 0;
            //if (terminating) break;
            if (ufds[i].revents & POLLIN) {
                if(ufds[i].fd == skfd) {
                    // do not accept connections after terminating signal
                    if (terminating == 0) {
                        len = sizeof(sin);
                        if ((newfd = accept(skfd, (struct sockaddr *) &sin, &len)) < 0) {
                            perror ("accept");
                            exit(3);
                        }
                        newclients++;
                        //ufds[nfds + newclients - 1] = (struct pollfd *) malloc(sizeof(struct pollfd));
                        ufds[nfds + newclients - 1].fd = newfd;
                        ufds[nfds + newclients - 1].events = POLLIN;
                    }
                }
                // if recv returns zero, that means the connection has been closed
                else if (recv(ufds[i].fd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) {
                    disconnect++;
                }
                else {
                    XDR handle_w, handle_r;
                    // attach socket to a stream
                    newfd = ufds[i].fd;
                    if ((stream = fdopen(newfd, "r+")) == (FILE *) - 1) {
                        perror("fdopen");
                        exit(1);
                    }
                    xdrstdio_create(&handle_w, stream, XDR_ENCODE);
                    xdrstdio_create(&handle_r, stream, XDR_DECODE);
                    xdr_char(&handle_r, &rcvinit);
                    printf("checking the rcvinit: %c\n", rcvinit);
                    switch(rcvinit) {
                        case 'r':
                            // no report when terminating
                            if (terminating) break;

                            // send perf info back
                            if (perf_head == NULL) {
                                printf("We have not found any perfect number yet\n");
                                sndinit = 'o';
                                xdr_char(&handle_w, &sndinit);
                            }
                            else {
                                sndinit = 'p';
                                xdr_char(&handle_w, &sndinit);
                                perf_end = 0;
                                perf_curr = perf_head;
                                while (perf_curr != NULL) {
                                    xdr_char(&handle_w, &perf_end);
                                    xdr_long(&handle_w, &(perf_curr->value));
                                    xdr_string(&handle_w, &(complist[perf_curr->comid - 1]->hostname), SIZELIMIT);
                                    perf_curr = perf_curr->next;
                                }
                                perf_end = 1;
                                xdr_char(&handle_w, &perf_end);
                            }
                            fflush(stream);
                            // send comp info back
                            if (comp_total == 0) {
                                printf("We have no compute registered yet\n");
                                sndinit = 'o';
                                xdr_char(&handle_w, &sndinit);
                                fflush(stream);
                                break;
                            }
                            sndinit = 'c';
                            xdr_char(&handle_w, &sndinit);
                            msg_end = 0;
                            for (i = 0; i < comp_total; i++) {
                                xdr_char(&handle_w, &msg_end);
                                xdr_int(&handle_w, &(complist[i]->id));
                                xdr_long(&handle_w, &(complist[i]->tested));
                                xdr_string(&handle_w, &(complist[i]->hostname), SIZELIMIT);
                                xdr_long(&handle_w, &(complist[i]->start));
                                xdr_char(&handle_w, &(complist[i]->stat));
                                xdr_long(&handle_w, &(complist[i]->end));
                            }
                            msg_end = 1;
                            xdr_char(&handle_w, &msg_end);
                            fflush(stream);
                            break;
                        case 't':
                            // no report when terminating
                            if (terminating) break;
                            // enter terminating process
                            terminating = 1;
                            printf("report -k is received\n");
                            // set its socket fd as waiting_terminate
                            waiting_terminate = newfd;
                            // send terminate signal out
                            /*
                            for (i = 0; i < comp_total; i++) {
                                FILE * stream_rt;
                                if ((stream_rt = fdopen(complist[i]->socketfd, "r+")) == (FILE *) -1) {
                                    perror("fdopen_caseT");
                                    exit(1);
                                }
                                XDR handle_wrt;
                                xdrstdio_create(&handle_wrt, stream_rt, XDR_ENCODE);
                                xdr_char(&handle_wrt, &rcvinit);
                                //fflush(stream_rt);
                            }*/
                            break;
                        case 'c':
                            // readin compid, start_req, timecost
                            xdr_int(&handle_r, &compid);
                            xdr_long(&handle_r, &start_req);
                            xdr_double(&handle_r, &timecost);
                            // no range assign when terminating
                            if (terminating) break;
                            // initial new compute
                            if (compid == 0) {
                                printf("new client coming\n");
                                compid = comp_total;
                                complist[compid] = (struct compent *) malloc (sizeof (struct compent));
                                complist[compid]->id = compid + 1;
                                complist[compid]->socketfd = newfd;
                                len = sizeof(tempsin);
                                getpeername(newfd, (struct sockaddr *) &tempsin, &len);
                                hostentp = gethostbyaddr((char *)&tempsin.sin_addr.s_addr, sizeof(tempsin.sin_addr.s_addr), AF_INET);
                                strcpy(complist[compid]->hostname, hostentp->h_name);
                                complist[compid]->tested=0;
                                complist[compid]->start=0;
                                complist[compid]->end=0;
                                complist[compid]->last=0;
                                complist[compid]->stat='N';
                                comp_total++;
                                comp_active++;
                                newrange = INITRANGE;
                            }
                            // calculate new range
                            else {
                                compid--;
                                newrange = (complist[compid]->end - complist[compid]->start + 1) * 15 / timecost;
                            }
                            // find start and end
                            range_curr = range_head;
                            while (range_curr != NULL) {
                                // after current range's end
                                if (start_req > range_curr->end) {
                                   // start_req located after the end range
                                   if (range_curr->next == NULL) {
                                       if (start_req == range_curr->end + 1) {
                                           range_curr->end = range_curr->end + newrange;
                                       }
                                       else {
                                           range_temp = (struct range *) malloc(sizeof (struct range));
                                           range_temp->start = start_req;
                                           range_temp->end = start_req + newrange - 1;
                                           range_temp->next = NULL;
                                           range_temp->prev = range_curr;
                                           range_curr->next = range_temp;
                                           range_curr = range_temp;
                                       }
                                       complist[compid]->start = start_req;
                                       complist[compid]->end = range_curr->end;
                                       break;
                                   }
                                   // calculate the distances between start_req and curr->end as well as next->end
                                   // start_req located between two existing ranges
                                   else if (start_req < range_curr->next->start) {
                                       range_temp = (struct range *) malloc(sizeof (struct range));
                                       range_temp->start = start_req;
                                       range_temp->end = start_req + newrange - 1;
                                       range_temp->next = NULL;
                                       range_temp->prev = NULL;
                                       if (start_req + newrange >= range_curr->next->start) {
                                           range_temp->end = range_curr->next->start - 1;
                                       }
                                       //else range_temp->end = start_req + newrange - 1;
                                       // new range
                                       complist[compid]->start = range_temp->start;
                                       complist[compid]->end = range_temp->end;
                                       // re-organize the range list
                                       if (range_temp->start - 1> range_curr->end && range_temp->end + 1 < range_curr->next->start) {
                                           range_temp->prev = range_curr;
                                           range_temp->next = range_curr->next;
                                           range_curr->next->prev = range_temp;
                                           range_curr->next = range_temp;
                                       }
                                       else if (range_temp->start - 1 == range_curr->end && range_temp->end + 1 == range_curr->next->start) {
                                           range_curr->end = range_curr->next->end;
                                           range_curr->next = range_curr->next->next;
                                       }
                                       else if (range_temp->start - 1 == range_curr->end) {
                                           range_curr->end = range_temp->end;
                                       }
                                       else range_curr->next->start = range_temp->start;
                                       break;
                                   }
                                   else range_curr = range_curr->next;  // go to the else block below in next cycle
                                }
                                // before current range's end
                                else {
                                    // start_req located before the head range
                                    if (range_curr->prev == NULL) {
                                        if (start_req + newrange >= range_curr->start) {
                                            complist[compid]->end = range_curr->start - 1;
                                            range_curr->start = start_req;
                                        }
                                        else {
                                            range_temp = (struct range *) malloc(sizeof (struct range));
                                            range_temp->start = start_req;
                                            range_temp->end = start_req + newrange - 1;
                                            range_temp->next = range_curr;
                                            range_temp->prev = NULL;
                                            range_curr->prev = range_temp;
                                            range_head = range_temp;
                                            complist[compid]->end = range_temp->end;
                                        }
                                        // new range is done
                                        complist[compid]->start = start_req;
                                        break;
                                    }
                                    // located in current range
                                    else {
                                        start_req = range_curr->end + 1;  // will be done in next cycle
                                    }
                                }
                            }
                            if (range_head == NULL) {
                                range_head = (struct range *) malloc (sizeof (struct range));
                                range_head->start = start_req;
                                range_head->end = start_req + newrange - 1;
                                range_head->next = NULL;
                                range_head->prev = NULL;
                                complist[compid]->start = start_req;
                                complist[compid]->end = start_req + newrange - 1;
                            }
                            // send back range
                            sndinit = 'n';
                            xdr_char(&handle_w, &sndinit);
                            xdr_long(&handle_w, &(complist[compid]->start));
                            xdr_int(&handle_w, &(complist[compid]->id));
                            xdr_long(&handle_w, &(complist[compid]->end));
                            fflush(stream);
                            break;
                        case 'p':
                            // readin compid and perfect number
                            xdr_int(&handle_r, &compid);
                            xdr_long(&handle_r, &newperf);
                            // put perf in to perf linked list
                            perf_temp = (struct perfent *) malloc (sizeof (struct perfent));
                            perf_temp->value = newperf;
                            perf_temp->comid = compid;
                            perf_temp->next = NULL;
                            if (perf_head == NULL) perf_head = perf_temp;
                            else {
                                perf_curr = perf_head;
                                while (perf_curr->next != NULL) {
                                    perf_curr = perf_curr->next;
                                }
                                perf_curr->next = perf_temp;
                            }
                            break;
                        case 'e':
                            // readin compid and last tested number for compute
                            xdr_int(&handle_r, &compid);
                            xdr_long(&handle_r, &last);
                            // update compute list
                            compid--;
                            complist[compid]->tested += last - complist[compid]->start + 1;
                            complist[compid]->last = last;
                            complist[compid]->stat = 'Y';
                            // update comp_active
                            comp_active--;
                            break;
                        default:
                            break;
                    }
                    //fflush(stream);
                }
            }
        }
        nfds += newclients;
    }
    return 0;
}
