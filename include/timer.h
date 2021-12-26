#include <unistd.h>
#include <sys/times.h>
#include <vector>

class timer {
private:
    vector<timer_ent> queue;
    vector<long> handles;
    vector<long> freehandles;
    long lasthandle;
public:
    timer() {
        lasthandle = 0;
    }

    void start(long &handle, long delay, (void callback())) {
    }
};
