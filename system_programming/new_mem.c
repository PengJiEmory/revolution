// THIS CODE IS MY OWN WORK, IT WAS WRITTEN WITHOUT CONSULTING
// A TUTOR OR CODE WRITTEN BY OTHER STUDENTS - PENG JI

/* CS551 Assignment 4 */
/* threadperf.c       */
/* Peng Ji            */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#define MAXTHREAD 20

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
const char *cmds[6] = {"start ", "idle ", "restart ", "wait ", "report", "quit"};
const int cmdlen[6] = {6, 5, 8, 5, 6, 4};
// CPU time
clock_t begin;
clock_t end;
// Elapsed time
time_t time_start;
time_t time_end;
// thread record
struct thread_record {
    pthread_t tid;
    int id;
    int start;
    int tested;
    int skipped;
    int found;
    int curblock;
    int idle;
};
struct thread_record *thread_rec[MAXTHREAD];
// perfect number record
int perfect_count = 0;
int perfect[50][2];
// shared parameters
int max;
int blocksize;
int block_count;
int thread_count = 0;
int totaltest=0;
char *bitmap;

int main (int argc, char *argv[]) {
    begin = clock();
    time_start = time(0);
    if (argc != 3) {
        printf("usage: threadperf MAX BLOCK\n");
        exit(1);
    }
    max = atoi(argv[1]); // max number to compute
    blocksize = atoi(argv[2]); // amount of numbers of each block
    // setup bitmap
    block_count = max/blocksize + 1;
    int char_count = block_count/8 + 1;
    bitmap = (char*)calloc(char_count, sizeof(char));
    int mode = block_count % 8;
    int i;
    if (mode >0) {
        for (i = 0; i < 7 - mode; i++) {
            bitmap[char_count - 1] = bitmap[char_count - 1] | 1<<(7 - i);
        }
    }

    // take advantage of multiple processors
    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
    // functions
    void showcmdlist();
    void terminate();
    void *compute();
    void report();
    char c;
    i = 0; // index of comand
    int j = 0; // index of char of comand
    int num = 0; // parameter for comand 0-3
    int numbegin = 0; // if num begins, wait for ' ' or EOF to end
    int numend = 0; // if num ends
    int nomatch=0;
    // parse comand
    while((c = fgetc(stdin)) != EOF) {
        if (c=='\n') {
            // show cmd list for wrong cmd
            if (nomatch || i > 5 || j < cmdlen[i] || (i < 4 && numbegin == 0)) showcmdlist();
            else {
                // execute the comand
                switch(i) {
                    case 0:
                        printf("new thread start with number %d\n", num);
                        if (thread_count < MAXTHREAD) {
                            pthread_mutex_lock(&mtx);
                            int tRecID = thread_count;
                            thread_count += 1;
                            pthread_mutex_unlock(&mtx);
                            thread_rec[tRecID] = (struct thread_record*)malloc(sizeof(struct thread_record));
                            thread_rec[tRecID]->id = tRecID + 1;
                            //printf("id is %d\n", thread_rec[tRecID]->id);
                            thread_rec[tRecID]->start = num;
                            thread_rec[tRecID]->tested = 0;
                            thread_rec[tRecID]->skipped = 0;
                            thread_rec[tRecID]->found = 0;
                            thread_rec[tRecID]->curblock = -1; //means unassigned
                            thread_rec[tRecID]->idle = 0;
                            //pthread_init(&thread_rec[tRecID]->tid, NULL,
                            //pthread_create(&thread_rec[tRecID]->tid, &tattr, compute, &tRecID);
                            pthread_create(&thread_rec[tRecID]->tid, NULL, compute, &tRecID);
                            //pthread_join(thread_rec[tRecID]->tid, NULL);
                            printf("thread %d craeted\n", tRecID);
                        }
                        else printf("%d threads are running, no more\n", thread_count);
                        break;
                    case 1:
                        printf("setting thread %d to idle\n", num);
                        if (num > thread_count) {
                            printf("oops, we have only %d threads\n", thread_count);
                        }
                        else thread_rec[num - 1]->idle = 1;
                        break;
                    case 2:
                        printf("restarting thread %d\n", num);
                        if (num > thread_count) {
                            printf("oops, we have only %d threads\n", thread_count);
                        }
                        else {
                            thread_rec[num - 1]->idle = 0;
                            pthread_cond_signal(&cond);
                        }
                        break;
                    case 3:
                        printf("I need to sleep %d sec\n", num);
                        sleep(num);
                        break;
                    case 4:
                        printf("here is the report\n");
                        report();
                        break;
                    case 5:
                        printf("Have a good day\n");
                        terminate();
                        break;
                    default:
                        ;
                        //showcmdlist();
                }
            }
            i = 0;
            j = 0;
            num = 0;
            numbegin = 0;
            numend = 0;
            nomatch = 0;
            continue;
        }
        if (i==6 | nomatch) continue;
        // the beginning letter
        if (j == 0) {
             if (c == ' ') continue;
             while (i < 6 && c != cmds[i][j]) {
                 i++;
             }
             if (i < 6) j = 1;
        }
        // after letters
        else if (j < cmdlen[i]) {
            if (c != cmds[i][j]) {
                if (i == 2 && j == 2 && c == cmds[4][j]) {
                    i = 4;
                    j++;
                }
                else nomatch = 1;
            }
            else j++;
        }
        // j == cmdlen[i] means string matched, wait for numbers
        else {
            if (c >= '0' && c <='9') {
                if (numend) nomatch = 1;
                else {
                     numbegin = 1;
                     num *= 10;
                     num += c - '0';
                }
            }
            else if (c == ' ') {
                if (numbegin) numend = 1;
            }
            else nomatch = 1;
        }
    }
    //fseek(stdin, 0, SEEK_END); // in case there is data left in stdin
    //for (i = 0; i < thread_count; i++) {
    //    pthread_join(thread_rec[i]->tid, NULL);
    //}
    return 0;
}

void showcmdlist() {
    printf("----------------------------------\n");
    printf("The following comands are valid:\n");
    printf("start   K\n");
    printf("idle    N\n");
    printf("restart N\n");
    printf("wait    S\n");
    printf("report\n");
    printf("quit\n");
    printf("Now, try again!\n");
}

int tested(int blockID, char *bitmap) {
    int quot = blockID >> 3;
    int mod = blockID & 7;
    int ans = 0;
    pthread_mutex_lock(&mtx);
    if (bitmap[quot] & 1 << mod) ans = 1;
    else {
        bitmap[quot] = bitmap[quot] | 1 << mod;
    }
    pthread_mutex_unlock(&mtx);
    return ans;
}

int isperfect(int num) {
    int maxnum = num;
    int sum = 1;
    int i;
    for (i = 2; i < maxnum; i++) {
        if (!(maxnum%i)) sum+=i;
    }
    return (sum == maxnum);
}

void *compute(int *num) {
    int tRecIDcompute = *num;
    printf("Thread %d is computing\n", tRecIDcompute);
    int startnumber = thread_rec[tRecIDcompute]->start;
    int blockID = startnumber / blocksize;
    int checkedblock = 0;
    void terminate();
    while(checkedblock < block_count) {
        checkedblock += 1;
        if (tested(blockID, bitmap)) {
            thread_rec[tRecIDcompute]->skipped+=blocksize;
        }
        else {
            int start = blockID * blocksize;
            int end = start + blocksize;
            int i;
            thread_rec[tRecIDcompute]->curblock = blockID;
            if (end > max + 1) end = max + 1;
            for (i = start; i < end; i++) {
                totaltest += 1;
                thread_rec[tRecIDcompute]->tested+=1;
                if (isperfect(i)) {
                    pthread_mutex_lock(&mtx);
                    int perfid = perfect_count;
                    perfect_count += 1;
                    pthread_mutex_unlock(&mtx);
                    perfect[perfid][0] = i;
                    perfect[perfid][1] = thread_rec[tRecIDcompute]->id;
                }
                pthread_mutex_lock(&mtx);
                while (thread_rec[tRecIDcompute]->idle) {
                    pthread_cond_wait(&cond, &mtx);
                }
                pthread_mutex_unlock(&mtx);
            }

        }
        blockID += 1;
        blockID %= block_count;
    }
    terminate();
}

void report() {
    printf("Thread_ID  Tested  Skipped  CurBLK  IDLE\n");
    int i;
    for (i=0;i<thread_count;i++) {
        printf("%5d      %6d  %7d  %6d  %4d\n", thread_rec[i]->id, thread_rec[i]->tested, thread_rec[i]->skipped, thread_rec[i]->curblock, thread_rec[i]->idle);
    }
    printf("Perfect  FoundBy\n");
    for (i=0;i<perfect_count;i++) {
        printf("%7d  %7d\n", perfect[i][0], perfect[i][1]);
    }
}

void terminate() {
    end = clock();
    time_end = time(0);
    double cpu_time = (double)(end - begin) / CLOCKS_PER_SEC;
    int i;
   // for (i=0; i<thread_count; i++) {
   //     pthread_cancel(thread_rec[i]->tid);
   // }
    printf("perfect numbers: ");
    for (i=0; i < perfect_count; i++){
        printf("%d ", perfect[i][0]);
    }
    printf("\nTotal number tested:  %d\n", totaltest);
    printf("Total CPU time:       %f sec\n", cpu_time);
    printf("process elapsed time: %f sec\n", (double) (time_end - time_start));
    exit(0);
}
