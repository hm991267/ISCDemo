#include "Publisher.hpp"
#include "Subscriber.hpp"
#include "TopicService.hpp"

int main()
{
    TopicService ts;
    IpcSub<int> sub;
    IpcPub<int> pub;
    pub.publish("abcde");
    sub.subscribe("abcde");
    int a[10];
    int b;
    int *pp;
    sub.read(a);
    sub.read(b);
}