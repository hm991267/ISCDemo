#include <thread>

#include "Publisher.hpp"
#include "Subscriber.hpp"
#include "TopicService.hpp"

#include <stdio.h>
#include <chrono>

const char *topic = "/test";

int main()
{
    TopicService ts;
    BaseSubscriber<int> sub;
    BasePublisher<int> pub;

    std::thread a(
        [&]()
        {
            pub.publish(topic);
            int i = 0;
            while (1)
            {
                pub.write(++i);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        });
    a.detach();

    std::this_thread::sleep_for(std::chrono::seconds(1));   
    sub.subscribe(topic);
    while (1)
    {
        int i;
        if (sub.read(i) > 0)
            printf("recv %d\n", i);
    }
    return 0;
}