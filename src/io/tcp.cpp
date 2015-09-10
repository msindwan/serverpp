#include <spp/tcp.h>

using namespace spp;
using namespace std;

static unsigned int __stdcall tcp_listener(void* param)
{
    SOCKET sclients[SPP_MAX_NUM_CLIENTS],
           slisten,
           saccept,
           sclient;

    char headers[SPP_MAX_HEADER_SIZE];
    char ip_buffer[INET_ADDRSTRLEN];
    int i, recv_bytes, addrlen;
    struct sockaddr_in addr;

    TCPServerManager* manager;
    TCPServer* server;
    fd_set fd_read;

    server = (TCPServer*)param;
    slisten = server->get_socket();
    addrlen = sizeof(addr);
    manager = TCPServerManager::get_manager();

    for (i = 0; i < SPP_MAX_NUM_CLIENTS; i++)
        sclients[i] = 0;

    while (server->is_running())
    {
        FD_ZERO(&fd_read);

        FD_SET(slisten, &fd_read);
        
        for (i = 0; i < SPP_MAX_NUM_CLIENTS; i++)
        {
            sclient = sclients[i];

            if (sclient > 0)
                FD_SET(sclient, &fd_read);
        }     

        if (select(0, &fd_read, NULL, NULL, NULL) == SOCKET_ERROR)
            continue;

        if (FD_ISSET(slisten, &fd_read))
        {
            if ((saccept = accept(slisten, (struct sockaddr *)&addr, (socklen_t*)&addrlen)) < 0 )
            {
                manager->log(
                    TCPServerManager::ERR,
                    server->get_log_path().c_str(),
                    "Failed to accept an incoming connection. {%d}",
                    WSAGetLastError());
                
                continue;
            }
            
            for (i = 0; i < SPP_MAX_NUM_CLIENTS; i++)
            {
                if (sclients[i] == 0)
                {
                    sclients[i] = saccept;
                    break;
                }
            }
        }
        
        for (i = 0; i < SPP_MAX_NUM_CLIENTS; i++)
        {
            sclient = sclients[i];

            if (FD_ISSET(sclient, &fd_read))
            {
                recv_bytes = recv(sclient, headers, SPP_MAX_HEADER_SIZE, 0);

                if (recv_bytes == SOCKET_ERROR)
                {
                    if (WSAGetLastError() != WSAECONNRESET)
                    {
                        manager->log(
                            TCPServerManager::ERR,
                            server->get_log_path().c_str(),
                            "Failed to receive data. {%d}",
                            WSAGetLastError());
                    }
                }

                else if (recv_bytes != 0)
                {
                    headers[recv_bytes] = '\0';
                    HTTPRequest request(headers, recv_bytes);
                    
                    manager->log(
                        TCPServerManager::INFO,
                        server->get_log_path().c_str(),
                        "%s:%d %s %s",
                        inet_ntop(AF_INET, &(addr.sin_addr), ip_buffer, INET_ADDRSTRLEN),
                        ntohs(addr.sin_port),
                        request.get_method().c_str(),
                        request.get_uri().c_str());
                    
                    server->send_response(sclient, &request);
                }
                    
                closesocket(sclient);
                sclients[i] = 0;
            }
        }
    }

    return 0;
}

TCPServer::TCPServer(jToken* server)
    : m_stop(true)
{
    HTTPLocation* http_location;

    string slog,
           type,
           key;

    FILE* log;
    int i;
    
    jToken *location,
           *temp;

    jArray *locations;

    // Get server port.
    temp = jconf_get(server, "o", "port");

    if (temp == NULL || temp->type != JCONF_INT)
        throw TCPException();

    m_port = strtol((char*)temp->data, NULL, 10);
    
    // Get log path.
    temp = jconf_get(server, "o", "traffic_log");

    if (temp != NULL)
    {
        if (temp->type != JCONF_STRING)
            throw TCPException();

        m_log = string((char*)temp->data, jconf_strlen((char*)temp->data, "\""));

        fopen_s(&log, m_log.c_str(), "ab");

        if (log == NULL)
            throw TCPException();

        fclose(log);
    }

    // Set locations.
    temp = jconf_get(server, "o", "locations");

    if (temp == NULL || temp->type != JCONF_ARRAY)
        throw TCPException();

    locations = (jArray*)temp->data;

    for (i = 0; i < locations->end; i++)
    {
        // Get the type.
        location = jconf_get(temp, "aa", i, 0);

        if (location == NULL || location->type != JCONF_STRING)
            throw TCPException();

        type = string((char*)location->data, jconf_strlen((char*)location->data, "\""));

        // Get the uri.
        location = jconf_get(temp, "aa", i, 1);

        if (location == NULL || location->type != JCONF_STRING)
            throw TCPException();

        key = string((char*)location->data, jconf_strlen((char*)location->data, "\""));

        // Get the location object.
        location = jconf_get(temp, "aa", i, 2);

        if (location == NULL)
            throw TCPException();

        http_location = new HTTPLocation(location, server);
        m_locations.set_location(type.c_str(), key.c_str(), http_location);
    }
}

bool TCPServer::is_running(void)
{
    bool status;

    m_mtx_stop.aquire();

    status = !m_stop;

    m_mtx_stop.release();

    return status;
}

void TCPServer::start(void)
{
    struct sockaddr_in addr;

    if (is_running())
        return;
    
    m_slisten = socket(AF_INET, SOCK_STREAM, 0);

    if (m_slisten == INVALID_SOCKET)
    {
        throw TCPException();
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (::bind(m_slisten, (struct sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_slisten);
        throw TCPException();
    }

    if (listen(m_slisten, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(m_slisten);
        throw TCPException();
    }

#if defined(WIN32) || defined(_WIN32_)
    m_listener = (HANDLE)_beginthreadex(NULL, 0, &tcp_listener, this, 0, NULL);
#endif

    m_stop = false;
}

void TCPServer::send_response(SOCKET socket, HTTPRequest* request)
{
    HTTPLocation *location, *error;
    map<string, string> m_params;
    TCPServerManager* manager;
    string path, response;
    stringstream stream;
    char *file, *ext;
    size_t size;

    location = m_locations.get_location(request);
    manager = TCPServerManager::get_manager();
        
    if (location == NULL)
    {
        // process error
        error = m_locations.get_error("404");

        if (error == NULL)
        {
            file = "<h4>Server++</h4><div>404 Not Found</div>";
            size = strlen(file);
        }
        else
        {
            m_params["code"] = "404";
            path = error->get_path(&m_params);
            file = read_file(path.c_str(), &size);
            
            if (file == NULL)
            {
                file = "<h4>Server++</h4><div>404 Not Found</div>";
                size = strlen(file);
            }
        }

        stream << "HTTP/1.1 404 Not Found" << endl;
        stream << "Content-Type: text/html" << endl;
        stream << "Content-Length:" << size << endl;

        goto send;
    }

    if (location->is_proxied())
    {
        // TODO: 307 Temporary Redirect
        // return;
    }

    if (request->get_method() != "GET")
    {
        // TODO: Determine error type
        return;
    }

    path = location->get_path(request);
    file = read_file(path.c_str(), &size);
    
    if (file == NULL)
    {
        // process error
        file = "<h4>Server++</h4><div>500 Internal Server Error</div>";
        size = strlen(file);

        stream << "HTTP/1.1 500 Internal Server Error" << endl;
        stream << "Content-Type: text/html" << endl;
        stream << "Content-Length:" << size << endl;

        goto send;
    }

    ext = get_ext(path.c_str(), path.size());

    stream << "HTTP/1.1 200 OK"  << endl;
    stream << "Content-Type:"    << manager->get_type(ext) << endl;
    stream << "Content-Length:"  << size << endl;

send:
    stream << "Server: Server++" << endl;
    stream << "\r" << endl;
    response = stream.str();

    // TODO: Send All
    send(socket, response.c_str(), response.size(), 0);
    send(socket, file, size, 0);
}

void TCPServer::wait(void)
{
#if defined(WIN32) || defined(_WIN32_)
    WaitForSingleObject(m_listener, INFINITE);
#endif
}

void TCPServer::stop(void)
{
    m_mtx_stop.aquire();

    m_stop = true;
    closesocket(m_slisten);

    m_mtx_stop.release();
}

int TCPServer::get_num_processors(void)
{
    static int processors = 0;

    if (processors == 0)
    {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        processors = info.dwNumberOfProcessors;
    }

    return 1;
}

void TCPServerManager::add_server(TCPServer* listener)
{
    m_servers.push_back(listener);
}

void TCPServerManager::wait_for_servers(void)
{
    std::list<TCPServer*>::iterator it;

    for (it = m_servers.begin(); it != m_servers.end(); it++)
        (*it)->wait();
}

void TCPServerManager::start_servers(void)
{
    std::list<TCPServer*>::iterator it;

    for (it = m_servers.begin(); it != m_servers.end(); it++)
        (*it)->start();
}

void TCPServerManager::stop_servers(void)
{
    std::list<TCPServer*>::iterator it;

    for (it = m_servers.begin(); it != m_servers.end(); it++)
        (*it)->stop();
}

void TCPServerManager::add_type(string ext, string mime)
{
    m_mimes[ext] = mime;
}

string TCPServerManager::get_type(string ext)
{
    return m_mimes[ext];
}