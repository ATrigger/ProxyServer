//
// Created by kamenev on 09.01.16.
//

#ifndef POLL_EVENT_TIMERS_H
#define POLL_EVENT_TIMERS_H
#include <chrono>
#include <map>
#include <functional>
namespace io
{
    namespace timer
    {
    class timer_element;
        class timer_service
        {
        public:

            typedef std::chrono::steady_clock clock_t;
            timer_service();
            void add(timer_element*);
            void remove(timer_element*);
            bool empty() const;
            clock_t::time_point top() const;
            void process(clock_t::time_point);
        private:
            std::map<clock_t::time_point,timer_element*> queue;

        };
        class timer_element
        {
        public:
            typedef timer_service::clock_t clock_t;
            typedef std::function<void()> callback_t;
            timer_element();
            timer_element(callback_t);
            timer_element(timer_service&,clock_t::duration,callback_t);
            timer_element(timer_service&,clock_t::time_point,callback_t);
            ~timer_element();
            void setCallback(callback_t);
            void setParent(timer_service*);
            void recharge(clock_t::duration);
            void turnOff();
            void recharge(clock_t::time_point);
        private:
            timer_service *parent;
            clock_t::time_point wake;
            callback_t on_wake;
            friend class timer_service;
        };
    }
}
#endif //POLL_EVENT_TIMERS_H
