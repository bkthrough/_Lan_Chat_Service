#include "cser.h"
#include <pthread.h>

int main()
{
    SerSocket s("127.0.0.1");

    s.service();
    return 0;
}
