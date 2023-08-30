#include <thread>

#include "Publisher.hpp"
#include "Subscriber.hpp"
#include <chrono>
#include <errno.h>
#include <stdio.h>
#include <time.h>

const char *topic = "/test";

static int64_t getTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

struct SimMsgMD
{
    int64_t ts;
    int i;
    char msg[100];
    SimMsgMD()
    {
        i = 0;
        memset(msg, 0, sizeof(msg));
    }
};

struct SimMsgAlpha
{
    int64_t ts;
    int i;
    char msg[200];

    SimMsgAlpha()
    {
        i = 0;
        memset(msg, 0, sizeof(msg));
    }
};

struct SimMsgOrder
{
    int64_t ts;
    int i;
    char source[4];
    char msg[100];

    SimMsgOrder()
    {
        i = 0;
        memset(msg, 0, sizeof(msg));
    }
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("./mytest topic pub/sub role\n");
        return 0;
    }
    char path[1000];
    sprintf(path, "/dev/shm/%s", argv[1]);
    unlink(path);
    if (argc >= 4)
    {
        std::string role = argv[3];
        if (role == "md")
        {
            BasePublisher<SimMsgMD> pub;
            pub.publish(argv[1]);
            int i = 0;
            while (1)
            {
                SimMsgMD msg[10];
                for (int j = 0; j < 10; j++)
                {
                    msg[j].i = i++;
                    // sprintf(msg[j].msg, "test md %d\n", i);
                }
                for (int j = 0; j < 10; j++)
                {
                    msg[j].ts = getTime();
                }
                if (auto ret = pub.write(msg, 10); ret > 0)
                    printf("send %d\n", msg[9].i);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        else if (role == "local")
        {
        }
        else if (role == "sta")
        {
            BaseSubscriber<SimMsgMD> sub;
            BasePublisher<SimMsgAlpha> pub;
            sub.subscribe(argv[1]);
            pub.publish(argv[4]);
            while (1)
            {
                SimMsgMD msg[10];
                if (auto ret = sub.read(msg, 10); ret > 0)
                {
                    for (int j = 0; j < ret; j++)
                    {
                        if (msg[j].i % 16 == 0)
                        {
                            SimMsgAlpha alpha;
                            alpha.i = msg[j].i;
                            alpha.ts = getTime();
                            // sprintf(alpha.msg, "alpha %ld %d %ld", msg[j].ts, msg[j].i, alpha.ts);
                            if (auto ret = pub.write(&alpha, 1); ret > 0)
                            {
                            }
                        }
                    }
                }
            }
        }
        else if (role == "api")
        {
            BasePublisher<SimMsgOrder> pub;
            pub.publish("s5p2|224.0.0.3:20002");
            int lasti = 0;
            if (argc == 4)
            {
                BaseSubscriber<SimMsgMD> subMd;
                subMd.subscribe(argv[1]);
                SimMsgMD msg[10];
                while (1)
                {
                    size_t mdret = 0;
                    mdret = subMd.read(msg, 10);
                    if (mdret == 0)
                        continue;
                    
                    for (int i = 0; i < mdret; i++)
                    {
                        if (msg[i].i % 16 == 0)
                        {
                            SimMsgOrder order;
                            order.i = msg[i].i;
                            order.ts = getTime();
                            order.source[0] = 'm';
                            order.source[1] = 'd';
                            order.source[2] = '\0';
                            if (auto ret = pub.write(&order, 1); ret > 0)
                            {
                            }
                        }
                    }
                    
                }
            }
            else if (argc == 5)
            {
                BaseSubscriber<SimMsgAlpha> subAlpha;
                subAlpha.subscribe(argv[4]);
                SimMsgAlpha msg1;
                while (1)
                {
                    size_t alpharet = 0;
                    alpharet = subAlpha.read(&msg1, 1);
                    if (alpharet > 0)
                    {
                        SimMsgOrder order;
                        order.i = msg1.i;
                        order.ts = getTime();
                        order.source[0] = 's';
                        order.source[1] = 't';
                        order.source[2] = 'a';
                        order.source[3] = '\0';
                        if (auto ret = pub.write(&order, 1); ret > 0)
                        {
                        }
                    }
                }
            }
        }
    }
    else
    {
        BaseSubscriber<SimMsgMD> sub;
        BasePublisher<SimMsgMD> pub;
        std::string type = argv[2];
        if (type == "pub")
        {
            pub.publish(argv[1]);
            int i = 0;
            while (1)
            {
                SimMsgMD msg[10];
                for (int j = 0; j < 10; j++)
                {
                    msg[j].i = i++;
                    sprintf(msg[j].msg, "test msg %d\n", i);
                }
                if (auto ret = pub.write(msg, 10); ret > 0)
                    printf("send %d\n", ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        else if (type == "sub")
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            sub.subscribe(argv[1]);
            while (1)
            {
                int i;
                SimMsgMD msg[10];
                if (auto ret = sub.read(msg, 10); ret > 0)
                {
                    for (int j = 0; j < ret; j++)
                    {
                        printf("recv %d %s\n", msg[j].i, msg[j].msg);
                    }
                }
            }
        }
    }
    return 0;
}