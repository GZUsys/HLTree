#include "utree.h"
#include "zipfian.h"
#include "zipfian_util.h"
#include <cmath>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <thread>
#include <atomic>
#include <immintrin.h>
extern "C"
{
    #include <atomic_ops.h>
}  

typedef uint64_t            setkey_t;
typedef void*               setval_t;

#define DEFAULT_DURATION                5000
#define DEFAULT_INITIAL                 100
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  100
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE               0 
#define DEFAULT_UNBALANCED              0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX
#define DETECT_LATENCY
//#define UNIFORM
int initial =     DEFAULT_INITIAL; 
unsigned int levelmax;
uint64_t record[1000000];
std::vector<uint64_t> buffer;
std::vector<uint64_t> slen;
void read_data_from_file(char* file)
{
    long count = 0;

    FILE* fp = fopen(file, "r");
    if (fp == NULL) {
        exit(-1);
    }
    buffer.clear();

    printf("reading\n");
    while (1) {
        unsigned long long key;
        count = fscanf(fp, "%ld\n", &key);
        if (count != 1) {
            break;
        }
        
        buffer.push_back(key);
    }
    fclose(fp);
    printf("file closed\n");
}


void scan_data_from_file(char* file)
{
    long count = 0;

    FILE* fp = fopen(file, "r");
    if (fp == NULL) {
        exit(-1);
    }
    buffer.clear();

    printf("reading\n");
    while (1) {
        unsigned long long key;
        string str;
	int len=0;
        count = fscanf(fp, "%ld %s\n", &key,&str);
        if (count != 2) {
            break;
        }
	printf("op is %s\n",str[0]);
	if(str[0]=="s")
	{
	   printf("the len is %s\n",substr(str,3));
	   len=(int)substr(str,3);
	   printf("the len is %d\n",len);
	}
       // buffer.push_back(key);
    }
    fclose(fp);
    printf("file closed\n");
}


/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
/*
int global_id;
void *test(void *data) 
{
    int unext, last = -1;                                              
    setkey_t val = 0;                                                  
    insert_nb = 0;
    pthread_t thread = pthread_self();
    
    thread_data_t *d = (thread_data_t *)data;
    int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset[d->affinityNodeID]);
    if (ret)
      perror("pthread_setaffinity_np");
    start_addr = d->start_addr;
    curr_addr = start_addr;
    barrier_cross(d->barrier);                                         
    unext = (rand_range_re(&d->seed, 100) - 1 < d->update);            
#ifndef UNIFORM
    ZipfianGenerator zf(max_range);
#endif
    while (stop == 0) {

        if (unext) {   
#ifdef UNIFORM
                val = rand_range_re(&d->seed, d->range);

#else
                val = zf.Next();
#endif

#ifdef DETECT_LATENCY
                if (d->id == 1){
                    clock_gettime(CLOCK_MONOTONIC, &T1);
                }
#endif
                d->set->insert(val, (char*) val);
                
#ifdef DETECT_LATENCY
                if (d->id == 1){
                    clock_gettime(CLOCK_MONOTONIC, &T2);
                    latency = ((T2.tv_sec - T1.tv_sec) * 1000000000 + (T2.tv_nsec - T1.tv_nsec)) / 100;
                    record[latency] += 1;
                    insert_nb += 1;
                }
                
#endif
                d->nb_added++;
                last = val;
                d->nb_add++;
        } 
        else {                                                           
#ifdef UNIFORM
            if (d->alternate) {
                if (d->update == 0) {
                    if (last < 0) {
                        val = d->first;
                        last = val;
                    } else {                                           // last >= 0
                        val = rand_range_re(&d->seed, d->range);
                        last = -1;
                    }
                } else {                                               // update != 0
                    if (last < 0) {
                        val = rand_range_re(&d->seed, d->range);
                        //last = val;
                    } else {
                        val = last;
                    }
                }
            }	
            else val = rand_range_re(&d->seed, d->range);
#else
            val = zf.Next();
#endif
#ifdef DETECT_LATENCY
                if (d->id == 1){
                    clock_gettime(CLOCK_MONOTONIC, &T1);
                }
#endif
            if (d->set->search(val) != NULL) d->nb_found++;

#ifdef DETECT_LATENCY
                if (d->id == 1){
                    clock_gettime(CLOCK_MONOTONIC, &T2);
                    latency = ((T2.tv_sec - T1.tv_sec) * 1000000000 + (T2.tv_nsec - T1.tv_nsec)) / 100;
                    record[latency] += 1;
                    insert_nb += 1;
                }
                
#endif
            d->nb_contains++;
        }

     //    Is the next op an update? 
        if (d->effective)                                              // a failed remove/add is a read-only tx
            unext = ((100 * (d->nb_added + d->nb_removed)) < (d->update * (d->nb_add + d->nb_remove + d->nb_contains)));
        else                                                           // remove/add (even failed) is considered as an update
            unext = (rand_range_re(&d->seed, 100) - 1 < d->update);

    }
#ifdef DETECT_LATENCY
    if (d->id == 1){
        uint64_t cnt = 0;
        uint64_t nb_50 = insert_nb / 2;
        uint64_t nb_90 = insert_nb * 0.9;
        uint64_t nb_99 = insert_nb * 0.99;
        bool flag_50 = false, flag_90 = false, flag_99 = false;
        double latency_50, latency_90, latency_99;

        for (int i=0; i < 1000000 && !(flag_50 && flag_90 && flag_99); i++){
            cnt += record[i];
            if (!flag_50 && cnt >= nb_50){
                latency_50 = (double)i / 10.0;
                flag_50 = true;
            }
            if (!flag_90 && cnt >= nb_90){
                latency_90 = (double)i / 10.0;
                flag_90 = true;
            }
            if (!flag_99 && cnt >= nb_99){
                latency_99 = (double)i / 10.0;
                flag_99 = true;
            }
        }
        printf("medium latency is %.1lfus\n90%% latency is %.1lfus\n99%% latency is %.1lfus\n", latency_50, latency_90, latency_99);
    }
#endif
    return NULL;
}
*/

int main(int argc, char **argv)
{
        
/*
    while(1) {
        i = 0;
        c = getopt_long(argc, argv, "hAf:d:i:t:r:S:u:U:c:", long_options, &i);
        if(c == -1) break;
        if(c == 0 && long_options[i].flag == 0) c = long_options[i].val;

        switch(c) {
                case 0:
                    break;
                case 'h':
                    printf("intset -- STM stress test "
                                 "(skip list)\n\n"
                                 "Usage:\n"
                                 "  intset [options...]\n\n"
                                 "Options:\n"
                                 "  -h, --help\n"
                                 "        Print this message\n"
                                 "  -A, --Alternate\n"
                                 "        Consecutive insert/remove target the same value\n"
                                 "  -f, --effective <int>\n"
                                 "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
                                 "  -d, --duration <int>\n"
                                 "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                                 "  -i, --initial-size <int>\n"
                                 "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
                                 "  -t, --thread-num <int>\n"
                                 "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
                                 "  -r, --range <int>\n"
                                 "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
                                 "  -s, --seed <int>\n"
                                 "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
                                 "  -u, --update-rate <int>\n"
                                 "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
                                 "  -U, --unbalance <int>\n"
                                 "        Percentage of skewness of the distribution of values (default=" XSTR(DEFAULT_UNBALANCED) ")\n"
                                 "  -c, --conflict ratio <int>\n"
                                 "        Percentage of conflict among threads \n"
                                 );
                    exit(0);
                case 'A':
                    alternate = 1;
                    break;
                case 'f':
                    effective =  atoi(optarg);
                    break;
                case 'd':
                    duration =   atoi(optarg);
                    break;
                case 'i':
                    initial =    atoi(optarg);
                    break;
                case 't':
                    nb_threads = atoi(optarg);
                    break;
                case 'r':
                    range =      atol(optarg);
                    break;
                case 'S':
                    seed =       atoi(optarg);
                    break;
                case 'u':
                    update =     atoi(optarg);
                    break;
                case 'U':
                    unbalanced = atoi(optarg);
                    break;
                case 'c':
                    //simulate_conflict = true;
                    //max_range = NODE_MAX / 2 * (100.0 / atoi(optarg)
    char loading_file[100];
    sprintf(loading_file, "%s", "/root/HXY/ubtree/datafile/loada.csv");
    read_data_from_file(loading_file);
    memset(record, 0, sizeof(record));
    btree *bt;
//work_id=0;
    bt = new btree();
    initial=buffer.size();
   int i = 0;
    struct timeval start_time, end_time;
    uint64_t       time_interval;
  int	worker_thread_num=1;
int keys_per_thread=50000000;
/*
for (int ii = 0; ii < 50000; ii++) {
                            uint64_t kk = buffer[ii];
                            bt->insert(kk, (char*) kk);
                        }

*/

         std::thread threads[worker_thread_num];
                for (int kt = 0; kt < worker_thread_num; kt++) {
			
                    threads[kt] = std::thread([=]() {
			// work_id=kt;
                        int start = keys_per_thread * kt;
                        int end = start+keys_per_thread;
                        for (int ii = start; ii < end; ii++) {
                            uint64_t kk = buffer[ii];
                            bt->insert(kk, (char*) kk);
                        }
                        });
                }
                for (int t = 0; t < worker_thread_num; t++) threads[t].join();
printf("load is end\n");
/*
    printf("Level max    : %d\n",             bt->level());
long long clwb_count=0;
long long sfence_count=0;
for(int sk=0;sk<10;sk++){printf("the %d num is %lld,%lld\n",sk,clwb_num[sk],sfence_num[sk]);sfence_count=sfence_num[sk]+sfence_count;clwb_count=clwb_count+clwb_num[sk];}
printf("clbw and sfence num is    : %lld,%lld\n",             clwb_count,sfence_count);
*/
/*
	buffer.clear();
            sprintf(loading_file, "%s", "/root/HXY/ubtree/datafile/insert50m.csv");
            read_data_from_file(loading_file);
                gettimeofday(&start_time, NULL);
                for (int kt = 0; kt < worker_thread_num; kt++) {

                    threads[kt] = std::thread([=]() {
                         work_id=kt;
                        int start = keys_per_thread * kt;
                        int end = start+5000000;
                        for (int ii = start; ii < end; ii++) {
                            uint64_t kk = buffer[ii];
                            bt->insert(kk, (char*) kk);
                        }
                        });
                }
                for (int t = 0; t < worker_thread_num; t++) threads[t].join();

    gettimeofday(&end_time, NULL);
    time_interval = 1000000 * (end_time.tv_sec - start_time.tv_sec) + end_time.tv_usec - start_time.tv_usec;
    printf("Insert time_interval = %lu ns\n", time_interval * 1000);
    printf("average insert op = %lu ns\n",    time_interval * 1000 / initial);
    printf("Level max    : %d\n",             bt->level());
clwb_count=0;
sfence_count=0;
for(int sk=0;sk<10;sk++){printf("the %d num is %lld,%lld\n",sk,clwb_num[sk],sfence_num[sk]);sfence_count=sfence_num[sk]+sfence_count;clwb_count=clwb_count+clwb_num[sk];}
printf("clbw and sfence num is    : %lld,%lld\n",             clwb_count,sfence_count);
*/
               buffer.clear();
            sprintf(loading_file, "%s", "/root/HXY/ubtree/datafile/read50m.csv");
            read_data_from_file(loading_file);
gettimeofday(&start_time, NULL);
                for (int kt = 0; kt < worker_thread_num; kt++) {

                    threads[kt] = std::thread([=]() {
                        // work_id=kt;
                        int start = keys_per_thread * kt;
                        int end = start+keys_per_thread;
                        for (int ii = start; ii < end; ii++) {
                            uint64_t kk = buffer[ii];
                            bt->remove(kk);
//printf(" ");
                        }
                        });
                }
                for (int t = 0; t < worker_thread_num; t++) threads[t].join();

    gettimeofday(&end_time, NULL);
/*
         long long num;
         while(bt->list_head->key<1000012719839013158){num =bt->list_head->key; bt->list_head=bt->list_head->next;}
         printf("the leaf root key is %lld\n",num);   
	bt->remove(1000012719839013158);
	buffer.clear();
            sprintf(loading_file, "%s", "/root/HXY/ubtree/datafile/loada.csv");
            read_data_from_file(loading_file);     
*/

    time_interval = 1000000 * (end_time.tv_sec - start_time.tv_sec) + end_time.tv_usec - start_time.tv_usec;
    printf("delete time_interval = %lu ns\n", time_interval * 1000);
    printf("average delete op = %lu ns\n",    time_interval * 1000 / initial);
    printf("Level max    : %d\n",             bt->level());
    return 0;
}


