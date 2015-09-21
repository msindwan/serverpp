/**
 * Serverpp TCP
 *
 * Description: Defines a server instance using TCP sockets.
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#ifndef __SOCKET_SPP_H__
#define __SOCKET_SPP_H__

// SPP Socket constants.
#define SPP_MAX_HEADER_SIZE 1024

#if defined(SPP_WINDOWS)

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <process.h>

#elif defined(SPP_UNIX)

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

typedef SOCKET int

#endif

#include <errno.h>
#include <sstream>
#include "http.h"
#include "log.h"
#include "ssl.h"

namespace spp
{
    /**
     * TCPClient: A represenation of a client connection with its
     * TCP socket descripter and content buffer.
     */
    class TCPClient
    {
    public:
        // Constructors.
        TCPClient(SOCKET s, SSL* ssl = NULL)
            : socket(s),
            header_size(0),
            content_size(0),
            content(NULL),
            ssl(ssl){}

    public:
        // Socket functions.
        void close(void);
        int send(void);
        int recv(void);

    public:
        // Public data members.
        char headers[SPP_MAX_HEADER_SIZE],
            *content;

        SOCKET socket;
        size_t header_size,
               content_size;

        SSL* ssl;
    };

    /**
     * TCPServer: An entity for listening to incomming connections and
     * handling HTTP requests/responses using TCP sockets.
     */
    class TCPServer
    {
    public:
        // Constructor and destructor
        TCPServer(jToken*);
        ~TCPServer(void);

    public:
        // Getters and Setters.
        bool is_running(void);

    public:
        // Member functions.
        virtual status generate_response(TCPClient*, HTTPRequest*);
        virtual void start(void);
        virtual void wait(void);
        virtual void stop(void);
        virtual int run(void);

    public:
        // General TCPListener Exception
        class TCPException
        {
        public:
            TCPException(std::string m, int err = -1) : msg(m), ecode(err) {};
        
        public:
            const char* get_error_msg() { return this->msg.c_str(); };
            int get_errcode() { return ecode; }
        
        private:
            std::string msg;
            int ecode;
        };

    protected:
        HANDLE m_listener;
        SOCKET m_slisten;
        bool m_stop;
        int m_port;

    private:
        // Data members.
        std::list<HTTPLocation*> m_locations;
        std::string m_log, m_cert, m_ckey;
        HTTPUriMap m_uri_map;
        SSL_CTX* m_ssl_ctx;
        Lock m_mtx_stop;
    };

    /**
     * TCPServerManager: A single object that manages a collection of
     * TCPServers.
     */
    class TCPServerManager : public Logger
    {
    public:
        // Singleton instance (Singletons are not necessarily great, but this is a common use case).
        static TCPServerManager* get_manager()
        {
            static TCPServerManager instance;
            return &instance;
        }

    private:
        TCPServerManager(void) {}

    private:
        // Disable copying.
        TCPServerManager(const TCPServerManager&);
        TCPServerManager& operator=(const TCPServerManager&);

    public:
        // Getters and setters.
        std::list < TCPServer* > get_servers() { return m_servers; }

    public:
        // Member functions.
        void add_server(TCPServer*);
        void wait_for_servers(void);
        void start_servers(void);
        void stop_servers(void);

        void add_type(std::string, std::string);
        std::string get_type(std::string);

    private:
        // Data members.
        std::map <std::string, std::string> m_mimes;
        std::list < TCPServer* > m_servers;
        bool m_state;
    };
}

#endif