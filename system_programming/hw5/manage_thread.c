// THIS CODE IS MY OWN WORK, IT WAS WRITTEN WITHOUT CONSULTING
// A TUTOR OR CODE WRITTEN BY OTHER STUDENTS - PENG JI

/* CS551 Assignment 5 */
/* manage_thread.c           */
/* Peng Ji            */

/*
 * To do list:
 * use threads
 * manage, compute and report can not work together
 */
#include "perfect.h"
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int comp_active = 0;
int terminating = 0;
int report_k = 0;

struct perfent {
    long value;
    int comid;
    struct perfent * next;
};

struct perfent * perf_head = NULL;

struct compent {
    int id; // assocaite with hostname
    int socketfd;
   // char hostname[256];
    char * hostname;
    long tested;
    long start_req;
    long start;
    long end;
    long last;
    char stat;  //'Y' or 'N'
};

struct compent * complist[CLTCOUNT];  //compute record

struct range {
    long start;
    long end;
    struct range * prev;
    struct range * next;
};

struct range * range_head = NULL;

void terminate() {
    // send terminate signal out
    terminating = 1;
    printf("terminating ...\n");
    pthread_mutex_lock(&mtx);
    while (comp_active > 0) {
        pthread_cond_wait(&cond, &mtx);
    }
    pthread_mutex_unlock(&mtx);
    if (report_k == 0) exit(0);
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
    int disconnect = 0;
    long newperf;
    long start_req, last, newrange, oldrange;
    long start = 1;
    long end = 1;
    double timecost;
    char buffer[32]; //find recv
    char sndinit, rcvinit, perf_end, msg_end;
    pthread_t tid;
    sigset_t mask;
    FILE * stream;
    struct sigaction action;
    struct sockaddr_in sin; // structure for socket address
    struct sockaddr_in tempsin;
    struct hostent * hostentp;
    struct pollfd *ufds = (struct pollfd *) calloc(CLTCOUNT, sizeof(struct pollfd));

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

    // take advantage of multiple processors
    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);


    // set up IP addr and port number for bind
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(atoi(argv[1]));

    // get an internet socket for stream connections
    if ((skfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        exit(1);
    }

    // setsockopt to avoid unavailable socket
    int yes = 1;
    if (setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
    }

    // do the actual bind
    if (bind (skfd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        perror("bind");
        exit(2);
    }

    // allow a connection queue for up to CLTCOUNT clients
    listen(skfd, CLTCOUNT);

    // Now loop accepting connections
    while (1) {
        if (terminating) {
            printf("Sorry, no more connections is allowed, for we are terminating\n");
            continue;
        }
        int newskfd;
        if ((newskfd = accept(skfd, (struct sockaddr *) &sin, &len)) < 0) {
            perror("accept");
            exit(1);
        }
        stream = fdopen(newskfd, "r");
        hostentp = gethostbyaddr((char *)&sin.sin_addr.s_addr, sizeof(sin.sin_addr.s_addr), AF_INET);
        XDR handle_w, handle_r;
        xdrstdio_create(&handle_w, stream, XDR_ENCODE);
        xdrstdio_create(&handle_r, stream, XDR_DECODE);
        char rcvinit;
        xdr_char(&handle_r, &rcvinit);
        if (rcvinit == 'c') {
            xdr_long(&handle_r, &start_req);
            printf("registering new client...\n");
            complist[comp_total]->id = comp_total;
            complist[comp_total]->socketfd = newskfd;
            complist[comp_total]->hostname = hostentp->h_name;
            complist[comp_total]->tested = 0;
            complist[comp_total]->start_req = start_req;
            complist[comp_total]->start = 0;
            complist[comp_total]->end = 0;
            complist[comp_total]->last = 0;
            complist[comp_total]->stat = 'N';
            pthread_create(&tid, &tattr, compute, struct compent complist[comp_total]);
            printf("thread %d is created for compute %d\n", tid, comp_total);
            comp_total++;
        }
        else if (rcvinit == 'r') {
            report(newskfd);
        }
        else if (rcvinit == 't') {
            else {
                report_k = newskfd;
                terminate();
                report(newskfd);
                exit(0);
            }
        }
    }
}

void report (int num) {
    int skfd = num;
    char sndinit;
    char perf_end;
    char msg_end;
    struct perfent * perf_curr;
    struct range * range_curr;
    XDR handle_w;

    stream = fdopen(skfd, "w");
    xdrstdio_create(&handle_w, stream, XDR_ENCODE);
    // send perf info back
    if (perf_head == NULL) {
        printf("We have not found any perfect number yet\n");
        sndinit = 'o';
        xdr_char(&handle_w, &sndinit);
    }
    else {
        sndinit = 'p';
        xdr_char(&handle_w, &sndinit);
        perf_end = 'P';
        perf_curr = perf_head;
        while (perf_curr != NULL) {
            dr_char(&handle_w, &perf_end);
            xdr_long(&handle_w, &(perf_curr->value));
            xdr_string(&handle_w, &(complist[perf_curr->comid]->hostname), SIZELIMIT);
            fflush(stream);
            perf_curr = perf_curr->next;
        }
        perf_end = 1;
        xdr_char(&handle_w, &perf_end);
    }
    fflush(stream);
    // send comp info back
    if (comp_total == 0) {
        printf("We have no compute registered yet\n");
        sndinit = 'O';
        xdr_char(&handle_w, &sndinit);
        fflush(stream);
    }
    else {
        sndinit = 'c';
        xdr_char(&handle_w, &sndinit);
        msg_end = 'M';
        for (i = 0; i < comp_total; i++) {
            xdr_char(&handle_w, &msg_end);
            xdr_int(&handle_w, &(complist[i]->id));
            xdr_long(&handle_w, &(complist[i]->tested));
            xdr_string(&handle_w, &(complist[i]->hostname), SIZELIMIT);
            xdr_long(&handle_w, &(complist[i]->start));
            xdr_char(&handle_w, &(complist[i]->stat));
            xdr_long(&handle_w, &(complist[i]->end));
            fflush(stream);
        }
        msg_end= 1;
        xdr_char(&handle_w, &msg_end);
        fflush(stream);
    }
}

void * compute (struct compent *client) {
    int term_sent = 0;
    long span = INITRANGE;
    double timecost;
    char range = 'r';  // init signal to compute
    char terminate = 't';  // init signal to compute
    char rcvinit;
    XDR handle_w, handle_r;
    struct range * nextrange;

    pthread_mutex_lock(&mtx);
    comp_active++;
    pthread_mutex_unlock(&mtx);

    stream = fdopen(client->socketfd, "r+");
    xdrstdio_create(&handle_w, stream, XDR_ENCODE);
    xdrstdio_create(&handle_r, stream, XDR_DECODE);

    while (1) {
        if (terminating) {
            xdr_char(&handle_w, &terminate);
            term_sent = 1;
        }
        else {
            if (client->last != 0) span = (client->last - client->start) * 15 / timecost;
            nextrange = newrange(client->start_req, span);
            client->start = nextrange->start;
            client->end = nextrange->end;
            xdr_char(&handle_w, &range);
            xdr_long(&handle_w, &(client->start));
            xdr_long(&handle_w, &(client->end));
        }
        while (1) {
            if (terminating && term_sent == 0) {
                xdr_char(&handle_w, &terminate);
            }
            xdr_char(&handle_r, &rcvinit);
            if (rcvinit == 'p') {
                xdr_long(&handle_r, &newperf);
                // put perf in to perf linked list
                struct perfent * perf_temp = (struct perfent *) malloc (sizeof (struct perfent));
                perf_temp->value = newperf;
                perf_temp->comid = client->id;
                perf_temp->next = NULL;
                if (perf_head == NULL) perf_head = perf_temp;
                else {
                    struct perfent * perf_curr = perf_head;
                    while (perf_curr->next != NULL) {
                        perf_curr = perf_curr->next;
                    }
                    perf_curr->next = perf_temp;
                }
            }
            else if (rcvinit == 'n') break;
            else if (rcvinit == 't') client->tested++;
            else printf("wrong signal from compute %d\n", client->id + 1);
        }
        xdr_long(&handle_r, &(client->last));
        xdr_char(&handle_r, &(client->stat)); //'Y' or 'N'
        xdr_double(&handle_r, &timecost);

        if (client->stat == 'Y') {
            pthread_mutex_lock(&mtx);
            rangeupdate(client->end, client->last);
            comp_active--;
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&mtx);
            printf("compute %d is exited\n", client->id);
            pthread_exit();
        }
    }
}

void rangeupdate(long end, long last) {
    if (end != last) {
        struct range * range_curr = range_head;
        while (range_curr != NULL) {
            if (range_curr->end == end) {
                range_curr->end = last;
                break;
            }
            else range_curr = range_curr->next;
        }
    }
}

struct range * newrange(long start_req, long span) {
    struct range * range_temp = (struct range *) malloc (sizeof(struct range));
    if (range_head == NULL) {
        range_temp->start = start_req;
        range_temp->end = start_req + span - 1;
        range_temp->prev = NULL;
        range_temp->end = NULL;
        range_head = range_temp;
    }
    else {
        struct range * range_curr = range_head;
        while (range_curr != NULL) {
            // after current range's end
            if (start_req > range_curr->end) {
                // start_req is larger than the largest tested/testing number
                if (range_curr->next == NULL) {
                    range_temp->start = start_req;
                    range_temp->end = start_req + span - 1;
                    range_temp->next = NULL;
                    range_temp->prev = range_curr;
                    range_curr->next = range_temp;
                    break;
                }
                // start_req located between current end and next start
                else if (start_req < range_curr->next->start) {
                    range_temp->start = start_req;
                    range_temp->end = start_req + span - 1;
                    if (start_req + span > range_curr->next->start) {
                        range_temp->end = range_curr->next->start - 1;
                    }
                    range_temp->next = NULL;
                    range_temp->prev = range_curr;
                    range_curr->next = range_temp;
                    break;
                }
                else range_curr = range_curr->next;  // go to the else block below in next cycle
            }
            else {
                if (start_req < range_head->start) {
                    range_temp->start = start_req;
                    range_temp->end = start_req + span - 1;
                    if (start_req + span > range_head->start) range_temp->end = range_head->start -1;
                    range_temp->prev = NULL;
                    range_temp->next = range_head;
                    range_head = range_temp;
                    break;
                }
                // located in current range
                else {
                    start_req = range_curr->end + 1;
                }
            }
        }
    }
    return range_temp;
}
/* ----------------- old code ------------------------- */
    while (1) {
        if (terminating) {
            // broadcast the ending signal
            if (terminating == 1) {
                for (i = 0; i < comp_total; i++) {
                    if (complist[i]->stat == 'Y') continue;
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
            // if all computes have been terminated, return to report and die
            if (comp_active == 0) {
                // if there is waiting report, report and die; else die;
                if (report_k > 0) {
                    FILE * stream_t;
                    XDR handle_wt;
                    if ((stream_t = fdopen(report_k, "r+")) == (FILE *) -1) {
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
                        perf_end = 'P';
                        perf_curr = perf_head;
                        while (perf_curr != NULL) {
                            xdr_char(&handle_wt, &perf_end);
                            xdr_long(&handle_wt, &(perf_curr->value));
                            xdr_string(&handle_wt, &(complist[perf_curr->comid]->hostname), SIZELIMIT);
                            fflush(stream_t);
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
                    msg_end = 'M';
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
        // use poll to check for available fd
        rv = poll(ufds, nfds, 0);
        if (rv == -1) {
            perror("poll");
            exit(0);
        }
        if (rv == 0) continue;
        //if (rv == disconnect) continue;
        disconnect = 0;
        for (i = 0; i < nfds; i++) {
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
                        //ufds[nfds + newclients - 1] = (struct pollfd *) malloc(sizeof(struct pollfd));
                        ufds[nfds + newclients].fd = newfd;
                        ufds[nfds + newclients].events = POLLIN;
                        newclients++;
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
                    //printf("checking the rcvinit: %c\n", rcvinit);
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
                                perf_end = 'P';
                                perf_curr = perf_head;
                                while (perf_curr != NULL) {
                                    xdr_char(&handle_w, &perf_end);
                                    xdr_long(&handle_w, &(perf_curr->value));
                                    // temp
                                    //char * temp = "testing perf";
                                    //xdr_string(&handle_w, &temp, SIZELIMIT);
                                    xdr_string(&handle_w, &(complist[perf_curr->comid]->hostname), SIZELIMIT);
                                    fflush(stream);
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
                            msg_end = 'M';
                            for (i = 0; i < comp_total; i++) {
                                xdr_char(&handle_w, &msg_end);
                                xdr_int(&handle_w, &(complist[i]->id));
                                xdr_long(&handle_w, &(complist[i]->tested));
                                xdr_string(&handle_w, &(complist[i]->hostname), SIZELIMIT);
                                xdr_long(&handle_w, &(complist[i]->start));
                                xdr_char(&handle_w, &(complist[i]->stat));
                                xdr_long(&handle_w, &(complist[i]->end));
                                fflush(stream);
                            }
                            msg_end = 1;
                            xdr_char(&handle_w, &msg_end);
                            fflush(stream);
                            break;
                        case 't':
                            if (terminating) break;  // no report when terminating
                            terminate();
                            report_k = newfd;  // set its socket fd as report_k
                            break;
                        case '_f':
                            xdr_int(&handle_r, &compid);
                            xdr_long(&handle_r, &start_req);
                            xdr_double(&handle_r, &timecost);
                            if (terminating) break;  // no range assign when terminating
                            // initialize new compute
                            if (compid == 0) {
                                printf("registering new client...\n");
                                compid = comp_total;
                                complist[compid] = (struct compent *) malloc (sizeof (struct compent));
                                complist[compid]->id = compid;
                                complist[compid]->socketfd = newfd;
                                len = sizeof(tempsin);
                                getpeername(newfd, (struct sockaddr *) &tempsin, &len);
                                hostentp = gethostbyaddr((char *)&tempsin.sin_addr.s_addr, sizeof(tempsin.sin_addr.s_addr), AF_INET);
                                //strcpy(complist[compid]->hostname, hostentp->h_name);
                                complist[compid]->hostname = hostentp->h_name;
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
                                complist[compid]->last = complist[compid]->end;
                                oldrange = complist[compid]->end - complist[compid]->start + 1;
                                complist[compid]->tested += oldrange;
                                newrange = oldrange * 15 / timecost;
                            }
                            // find start and end
                            range_curr = range_head;
                            while (range_curr != NULL) {
                                // after current range's end
                                if (start_req > range_curr->end) {
                                    // start_req is larger than the largest tested
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
                                    // start_req located between current end and next start
                                    else if (start_req < range_curr->next->start) {
                                        range_temp = (struct range *) malloc(sizeof (struct range));
                                        range_temp->start = start_req;
                                        range_temp->end = start_req + newrange - 1;
                                        range_temp->next = NULL;
                                        range_temp->prev = NULL;
                                        if (start_req + newrange > range_curr->next->start) {
                                            range_temp->end = range_curr->next->start - 1;
                                        }
                                        //else range_temp->end = start_req + newrange - 1;
                                        // new range
                                        complist[compid]->start = range_temp->start;
                                        complist[compid]->end = range_temp->end;
                                        // re-organize the range list
                                        if ((range_temp->start - 1 > range_curr->end) && (range_temp->end + 1 < range_curr->next->start)) {
                                            range_temp->prev = range_curr;
                                            range_temp->next = range_curr->next;
                                            range_curr->next->prev = range_temp;
                                            range_curr->next = range_temp;
                                        }
                                        else if ((range_temp->start - 1 == range_curr->end) && (range_temp->end + 1 == range_curr->next->start)) {
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
                                // start_req before current range's end
                                else {
                                    // located in current range
                                    if (start_req >= range_curr->start) start_req = range_curr->end + 1;
                                    // start_req located before the head range
                                    else if (start_req + newrange >= range_curr->start) {
                                        complist[compid]->start = start_req;
                                        complist[compid]->end = range_curr->start - 1;
                                        range_curr->start = start_req;
                                        break;
                                    }
                                    else {
                                        range_temp = (struct range *) malloc(sizeof (struct range));
                                        range_temp->start = start_req;
                                        range_temp->end = start_req + newrange - 1;
                                        range_temp->next = range_curr;
                                        range_temp->prev = NULL;
                                        range_curr->prev = range_temp;
                                        range_head = range_temp;
                                        complist[compid]->start = start_req;
                                        complist[compid]->end = range_temp->end;
                                        break;
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
                            printf("%d\n", newperf);
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
void * perfect(struct clientinfo * client) {
    FILE * stream;
    int skfd = client->sock_fd;
    int id = client->comp_id;
    skfd = client_sock_fd;
    if ((stream = skfd, "r+")) == (FILE *) -1) {
        perror("fdopen_caseT");
        exit(1);
    }

}
