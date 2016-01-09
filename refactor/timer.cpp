//
// Created by kamenev on 09.01.16.
//

#include "timer.h"
#include "debug.h"
io::timer::timer_service::timer_service()
{

}
void io::timer::timer_service::add(io::timer::timer_element *element)
{
    queue.insert({element->wake,element});
}
void io::timer::timer_service::remove(io::timer::timer_element *element)
{
    auto i = queue.find(element->wake);
    if(i!=queue.end())
        queue.erase(i);
}
bool io::timer::timer_service::empty() const
{
    return queue.empty();
}
io::timer::timer_service::clock_t::time_point io::timer::timer_service::top() const
{
    return queue.begin()->first;
}
void io::timer::timer_service::process(clock_t::time_point point)
{
    for(;;){
        if(empty()) break;
        if(queue.begin()->first > point) break;
        auto nearest = queue.begin();
        try{
            nearest->second->on_wake();
        }
        catch(std::exception &e){
            LOG("Couldn't process timer: %s",e.what());
        }
        catch(...){
            INFO("Couldn't process timer due to an unknown error");
        }
        remove(nearest->second);
    }
}
io::timer::timer_element::timer_element():parent(nullptr)
{}
io::timer::timer_element::timer_element(io::timer::timer_element::callback_t t)
    :parent(nullptr),on_wake(std::move(t))
{}
io::timer::timer_element::timer_element(io::timer::timer_service &service,
                                        clock_t::duration duration,
                                        io::timer::timer_element::callback_t t)
    :parent(&service),wake(clock_t::now() + duration),on_wake(std::move(t))
{
    parent->add(this);
}
io::timer::timer_element::timer_element(io::timer::timer_service &service,
                                        clock_t::time_point point,
                                        io::timer::timer_element::callback_t t)
    :parent(&service),wake(point),on_wake(std::move(t))
{
    parent->add(this);
}
io::timer::timer_element::~timer_element()
{
    if(parent) parent->remove(this);
}
void io::timer::timer_element::setCallback(io::timer::timer_element::callback_t t)
{
    on_wake = std::move(t);
}
void io::timer::timer_element::setParent(timer_service* parent){
    this->parent = parent;
}
void io::timer::timer_element::recharge(clock_t::duration duration)
{
    if(!parent) return;
    parent->remove(this);
    wake = clock_t::now()+duration;
    parent->add(this);
}
void io::timer::timer_element::recharge(clock_t::time_point point)
{
    if(!parent) return;
    parent->remove(this);
    wake = point;
    parent->add(this);
}
void io::timer::timer_element::turnOff()
{
    if(!parent) return;
    parent->remove(this);
}
