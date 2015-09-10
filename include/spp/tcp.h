/**
 * Serverpp TCP
 *
 * Description: Defines a server instance using TCP sockets.
 * Author: Mayank Sindwani
 * Date: 2015-09-10
 */

#ifndef __SOCKET_SPP_H__
#define __SOCKET_SPP_H__

// SPP Socket constants.
#define SPP_MAX_HEADER_SIZE 8024
#define SPP_MAX_NUM_CLIENTS 1000

#if defined(_WIN32) || defined(WIN32)
	
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <process.h>

#elif defined(__unix__)

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

typedef SOCKET int

#endif

#include <sstream>
#include "http.h"
#include "log.h"

namespace spp
{
    /**
     * TCPServer: An entity for listening to incomming connections and
     * handling HTTP requests/responses.
     */
    class TCPServer
    {
    public:
        // Constructor and destructor
        TCPServer(jToken*);

    public:
        // Getters and Setters.
        std::string get_log_path(void) { return m_log; }
        SOCKET get_socket(void) { return m_slisten; }
        bool is_running(void);

    public:
        // Member functions.
        virtual void send_response(SOCKET, HTTPRequest*);
        virtual void start(void);
        virtual void wait(void);
        virtual void stop(void);
                
        // General TCPListener Exception
        class TCPException
        {
        public:
            TCPException() {};
        public:
            int get_errcode() {
                return this->m_errcode;
            };
        private:
            int m_errcode;
        };

    public:
        // Public helper functions.
        static int get_num_processors();

    protected:
        HANDLE m_listener;
        SOCKET m_slisten;
        bool m_stop;
        int m_port;

    private:
        // Data members.
        Lock m_mtx_stop, m_mtx_files;
        HTTPUriMap m_locations;
        std::string m_log;

        
    };

    /**
     * TCPServerManager: A single object that manages a collection of
     * TCPServers.
     */
    class TCPServerManager : public Logger
    {
    public:
        // Singleton instance.
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