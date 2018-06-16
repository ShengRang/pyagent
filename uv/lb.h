#ifndef LB_H
#define LB_H


#define MAX_EDS 10

typedef struct lb_entry{
    char *host;
    int port;
    int weight;
    unsigned int hash;
}lb_entry;

struct etcd_lb{
    lb_entry ends[MAX_EDS];
    int rate[MAX_EDS];
    int ecnt;
    int rv;
    int prev;
    int maxSum;
    etcd_lb (): ecnt(0) {}
    void insert(char *host, int port, int weight) {
        ends[ecnt].host = host;
        ends[ecnt].hash = 0;
        ends[ecnt].port = port;
        ends[ecnt++].weight = weight;
    }
    void init_rate() {
        rate[0] = ends[0].weight;
        for(int i=1; i<ecnt; i++)
            rate[i] = rate[i-1] + ends[i].weight;
        maxSum = rate[ecnt-1];
        rv = 0; prev = 0;
    }
    int nextIdx() {
        int res = rv;
        prev = (prev + 1) % maxSum;
        if(prev == 0)
            rv = 0;
        else if (prev >= rate[rv])
            rv = (rv + 1) % ecnt;
        return res;
    }
};

#endif
