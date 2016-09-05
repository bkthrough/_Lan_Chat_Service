#include <iostream>
#include <map>
#include <list>
#include <stdlib.h>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "data.h"
using namespace std;

pthread_cond_t cond;
pthread_mutex_t mutex;
void sig_end(int sig)
{
}
void *pthread_fun(void *arg)
{
    int *i = (int *)arg;
    pthread_cond_wait(&cond, &mutex);
    cout << "signal" << endl;
}
int main()
{
    pthread_t tid[4];
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    for(int i = 0; i < 4; ++i){
        pthread_create(&tid[i], NULL, pthread_fun, (void *)i);
    }
    //pthread_cond_signal(&cond);
    for(int i = 0; i < 4; ++i){
        pthread_join(tid[i], NULL);
    }

    return 0;
}
