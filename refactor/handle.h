//
// Created by kamenev on 29.01.16.
//

#ifndef POLL_EVENT_RAII_FD_H
#define POLL_EVENT_RAII_FD_H


class handle
{
public:
    handle();
    handle(int fd);
    ~handle();
    void close();
    int get_raw() const;
private:
    int raw = -1;
};


#endif //POLL_EVENT_RAII_FD_H
