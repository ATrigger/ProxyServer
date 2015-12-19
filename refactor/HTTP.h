//
// Created by kamenev on 24.11.15.
//

#ifndef POLL_EVENT_HTTP_H
#define POLL_EVENT_HTTP_H


#include <unordered_map>
#include <string>
#include <boost/algorithm/string.hpp>

#include <sstream>
#include <regex>
#include <iostream>
class HTTP
{
public:
    static std::unordered_map<std::string, std::string> parse(std::string in)
    {
        std::unordered_map<std::string, std::string> m;

        std::istringstream resp(in);
        std::string header;
        std::string::size_type index;
        while (std::getline(resp, header) && header != "\r") {
            index = header.find(':', 0);
            if (index != std::string::npos) {
                m.insert(std::make_pair(
                    boost::algorithm::trim_copy(header.substr(0, index)),
                    boost::algorithm::trim_copy(header.substr(index + 1))
                ));
            }
        }
        return m;
    }
    static std::string placeholder()
    {
        std::string request = std::string(
            "HTTP/1.1 400 Bad Request\r\nServer: shit\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 166\r\nConnection: close\r\n");
        std::ostringstream out(request);
        out << "\r\n";
        out << "<html>";
        out << "\r\n";
        out << "<head><title>400 Bad Request</title></head>";
        out << "\r\n";
        out << "<body bgcolor=\"white\">";
        out << "\r\n";
        out << "<center><h1>400 Bad Request</h1></center>";
        out << "\r\n";
        out << "<hr><center>proxy</center>";
        out << "\r\n";
        out << "</body>";
        out << "\r\n";
        out << "</html>";
        request += out.str();
        return request;
    }
    static std::string notFound()
    {
        std::string request = std::string(
            "HTTP/1.1 404 Not found\r\nServer: shit\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 160\r\nConnection: close\r\n");
        std::ostringstream out(request);
        out << "\r\n";
        out << "<html>";
        out << "\r\n";
        out << "<head><title>404 Not found</title></head>";
        out << "\r\n";
        out << "<body bgcolor=\"white\">";
        out << "\r\n";
        out << "<center><h1>404 Not found</h1></center>";
        out << "\r\n";
        out << "<hr><center>proxy</center>";
        out << "\r\n";
        out << "</body>";
        out << "\r\n";
        out << "</html>";
        request += out.str();

        return request;
    }
    HTTP(std::string input)
        : text(input)
    { };
    void add_part(std::string);
    virtual ~HTTP()
    { };
    int get_state()
    { return state; };
    std::string get_header(std::string) const;
    std::string get_body() const
    { return body; }
    std::string get_text() const
    { return text; }
    enum state_t
    {
        FAIL = -1, BEFORE = 0, FIRSTLINE = 1, HEADERS = 2, BODYPART = 3, BODYFULL = 4
    };
    state_t state = BEFORE;
protected:
    void update_state();
    void check_body();
    void parse_headers();
    virtual void parse_first_line() = 0;

    size_t body_start = 0;
    std::string text;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};
struct request: public HTTP
{
    request(std::string text)
        : HTTP(text)
    { update_state(); };

    std::string get_method() const
    { return method; }
    std::string get_URI();
    std::string get_host();
    std::string get_request_text();


private:
    void parse_first_line() override;

    std::string method;
    std::string URI;
    std::string http_version;
    std::string host = "";
};

struct response: public HTTP
{
    response(std::string text)
        : HTTP(text)
    { update_state(); };

    std::string get_code() const
    { return code; }

private:
    void parse_first_line() override;

    std::string code;
    std::string http_version;
};

#endif //POLL_EVENT_HTTP_H