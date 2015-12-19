#include "echo_server.h"
#include "debug.h"

echo_server::bind::bind(echo_server *parent)
    : parent(parent)
    , socket(parent->ss.accept([this] {
        LOG("Disconnected sock %d",this->socket.getFd());
        this->parent->connections.erase(this);
    }))
    , start_offset()
    , end_offset()
{
    update();
}

void echo_server::bind::update()
{
    if (end_offset == 0)
    {

        socket.setOn_read([this] {
            end_offset = socket.read_over_connection(buf, sizeof buf);
            LOG("Got %d bytes",end_offset);
            update();
        });
        socket.setOn_write(connection::callback());
    }
    else
    {

        socket.setOn_read(connection::callback());
        socket.setOn_write([this] {
            start_offset += socket.write_over_connection(buf + start_offset, end_offset - start_offset);
            if (start_offset == end_offset)
            {
                start_offset = 0;
                end_offset = 0;
                update();
            }
        });
    }
}



echo_server::echo_server(io::io_service &ep, ipv4_endpoint const& local_endpoint)
    : ss{ep, local_endpoint, std::bind(&echo_server::on_new_connection, this)}
{}

ipv4_endpoint echo_server::local_endpoint() const
{
    return ss.local_endpoint();
}

void echo_server::on_new_connection()
{
    std::unique_ptr<bind> cc(new bind(this));
    bind* pcc = cc.get();
    connections.emplace(pcc, std::move(cc));
}
