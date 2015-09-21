/**
 * Serverpp TCP Implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#include <spp/tcp.h>

using namespace spp;
using namespace std;

/**
 * TCPListener
 *
 * @description Thread callback to run a server.
 * @param {param} // The server instance.
 */
static unsigned int __stdcall tcp_listener(void* param)
{
    // Run the provided server instance.
    TCPServer* server;
    server = (TCPServer*)param;
    return server->run();
}

/**
 * TCPClient::close
 *
 * @description Closes a tcp client.
 */
void TCPClient::close(void)
{
    closesocket(socket);

    // Shutdown SSL.
    if (ssl)
    {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    delete content;
}

/**
 * TCPClient::recv
 *
 * @description Recieves data.
 * @returns // The number of bytes read.
 */
int TCPClient::recv(void)
{
    int read_bytes;

    // If ssl is enabled, call SSL_read. Otherwise, call recv.
    read_bytes = ssl ?
        SSL_read(ssl,  headers + header_size, SPP_MAX_HEADER_SIZE - header_size) :
        ::recv(socket, headers + header_size, SPP_MAX_HEADER_SIZE - header_size, 0);

    return read_bytes;
}

/**
 * TCPClient::send
 *
 * @description Sends data.
 * @returns // The number of bytes sent.
 */
int TCPClient::send(void)
{
    int sent_bytes;
    sent_bytes = 0;

    // Send the header.
    if (header_size > 0)
    {
        sent_bytes = ssl ?
            SSL_write(ssl, (char*)headers, header_size) :
            ::send(socket, headers, header_size, 0);

        header_size -= sent_bytes;
    }

    // Send the content.
    if (content_size > 0)
    {
        sent_bytes = ssl ?
            SSL_write(ssl, (char*)content, content_size) :
            ::send(socket, content, content_size, 0);

        content_size -= sent_bytes;
    }

    return sent_bytes;
}

/**
 * TCPServer constructor.
 *
 * @param {server} // The server configuration.
 */
TCPServer::TCPServer(jToken* server)
    : m_stop(true),
      m_ssl_ctx(NULL)
{
    HTTPLocation* http_location;
    jToken *location_cert,
           *location_key,
           *location,
           *ssl_token,
           *temp;

    jArray *locations;

    string slog,
           type,
            key;

    char error_msg[80];
    int i, rtn;
    FILE* log;

    if (server->type != JCONF_OBJECT)
        throw TCPException("Server must be an object.");
    
    // Get server port.
    temp = jconf_get(server, "o", "port");

    if (temp == NULL || temp->type != JCONF_INT)
        throw TCPException("An integer port is required.");

    m_port = strtol((char*)temp->data, NULL, 10);

    // Get log path.
    temp = jconf_get(server, "o", "traffic_log");

    if (temp != NULL)
    {
        if (temp->type != JCONF_STRING)
            throw TCPException("Traffic log must be a string.");

        m_log = string((char*)temp->data);

#if defined(_MSC_VER)
        fopen_s(&log, m_log.c_str(), "ab");
#else
        log = fopen(m_log.c_str(), "ab");
#endif

        if (log == NULL)
            throw TCPException("Failed to open " + m_log);

        fclose(log);
    }

    // Set SSL context.
    temp = jconf_get(server, "o", "ssl");

    if (temp != NULL)
    {
        // Get the SSL properties.
        ssl_token = jconf_get(temp, "o", "enabled");

        if (ssl_token == NULL || ssl_token->type == JCONF_TRUE)
        {
            // Get the certificate.
            location_cert = jconf_get(temp, "o", "cert");

            if (location_cert == NULL || location_cert->type != JCONF_STRING)
                throw TCPException("An SSL certificate path is required.");

            m_cert = string((char*)location_cert->data);

            // Get the key.
            location_key = jconf_get(temp, "o", "key");

            if (location_key == NULL || location_key->type != JCONF_STRING)
                throw TCPException("An SSL key path is required.");

            m_ckey = string((char*)location_key->data);

            // Set the SSL context.
            m_ssl_ctx = SSL_CTX_new(SSLv23_server_method());
            SSL_CTX_set_options(m_ssl_ctx, SSL_OP_SINGLE_DH_USE);

            if ((rtn = load_certificates(m_ssl_ctx, m_cert.c_str(), m_ckey.c_str())) < 0)
                throw TCPException("Failed to load the SSL certificates.");
        }
    }

    // Set locations.
    temp = jconf_get(server, "o", "locations");

    if (temp == NULL || temp->type != JCONF_ARRAY)
        throw TCPException("An array for locations is required.");

    locations = (jArray*)temp->data;

    for (i = 0; i < locations->end; i++)
    {
        // Get the type.
        location = jconf_get(temp, "aa", i, 0);
        sprintf(error_msg, "Invalid location at index %d", i);

        if (location == NULL || location->type != JCONF_STRING)
            throw TCPException(error_msg);

        type = string((char*)location->data);

        // Get the uri.
        location = jconf_get(temp, "aa", i, 1);

        if (location == NULL || location->type != JCONF_STRING)
            throw TCPException(error_msg);

        key = string((char*)location->data);

        // Get the location object.
        location = jconf_get(temp, "aa", i, 2);

        if (location == NULL)
            throw TCPException(error_msg);

        try
        {
            http_location = new HTTPLocation(location, server);
            m_locations.push_back(http_location);
            m_uri_map.set_location(type.c_str(), key.c_str(), http_location);
        }
        catch (HTTPException)
        {
            throw TCPException(error_msg);
        }
    }
}

/**
 * TCPServer destructor.
 */
TCPServer::~TCPServer(void)
{
    std::list< HTTPLocation* >::iterator it;

    for (it = m_locations.begin(); it != m_locations.end(); it++)
        delete *it;

    if (m_ssl_ctx)
        SSL_CTX_free(m_ssl_ctx);
}

/**
 * TCPServer::start
 *
 * @description Starts the listening socket and dispatches the worker thread.
 */
void TCPServer::start(void)
{
    struct sockaddr_in addr;
    char error_msg[80];
    int err;
    
    if (is_running())
        return;

    // Create the listening socket.
    if ((m_slisten = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
#if defined(SPP_WINDOWS)
        err = WSAGetLastError();
#elif defined(SPP_LINUX)
        // TODO
#endif
        throw TCPException("Failed to initialize the listening socket.", err);
    }

    // Bind to the loopback IP and provided port.
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    // Bind the listening socket.
    if (::bind(m_slisten, (struct sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR)
    {
        sprintf(error_msg, "Failed to bind the listening socket on port %d.", m_port);
#if defined(SPP_WINDOWS)
        err = WSAGetLastError();
#elif defined(SPP_LINUX)
        // TODO
#endif
        closesocket(m_slisten);
        throw TCPException(error_msg, err);
    }

    // Start listening.
    if (listen(m_slisten, SOMAXCONN) == SOCKET_ERROR)
    {
#if defined(SPP_WINDOWS)
        err = WSAGetLastError();
#elif defined(SPP_LINUX)
        // TODO
#endif
        closesocket(m_slisten);
        throw TCPException("Listen failed.", err);
    }

    // Start the worker thread.
#if defined(SPP_WINDOWS)
    m_listener = (HANDLE)_beginthreadex(NULL, 0, &tcp_listener, this, 0, NULL);
#elif defined(SPP_LINUX)
    // TODO
#endif

    m_stop = false;
}

/**
 * TCPServer::is_running
 *
 * @description Returns a flag indicating if the server is running.
 * @returns // True if it is running; false otherwise.
 */
bool TCPServer::is_running(void)
{
    bool status;

    m_mtx_stop.aquire();

    status = !m_stop;

    m_mtx_stop.release();

    return status;
}

/**
 * TCPServer::run
 *
 * @description: Accepts incomming connections and serves content.
 * @returns // The termination status.
 */
int TCPServer::run(void)
{
    fd_set fd_read, fd_write, fd_except;
    list<TCPClient>::iterator it;
    TCPServerManager* manager;
    list<TCPClient> clients;
    socklen_t addrlen;
    sockaddr_in addr;
    SOCKET sclient;
    DWORD timeout;
    int err;
    u_long mode;
    SSL* ssl;

    char ip_buffer[INET_ADDRSTRLEN];
    int recv_bytes,
        send_bytes,
        errlen;

    manager = TCPServerManager::get_manager();
    addrlen = sizeof(addr);
    errlen = sizeof(err);
    timeout = 1000;
    mode = 1;

    while (is_running())
    {
        // Initialize the fd_sets.
        FD_ZERO(&fd_read);
        FD_ZERO(&fd_write);
        FD_ZERO(&fd_except);

        FD_SET(m_slisten, &fd_read);
        FD_SET(m_slisten, &fd_except);

        for (it = clients.begin(); it != clients.end(); it++)
        {
            // Space available for read buffer.
            if (it->header_size < SPP_MAX_HEADER_SIZE)
                FD_SET(it->socket, &fd_read);

            // Data required to be sent.
            if (it->header_size > 0 || it->content_size > 0)
                FD_SET(it->socket, &fd_write);

            FD_SET(it->socket, &fd_except);
        }

        // Wait for socket activity.
        if (select(0, &fd_read, &fd_write, &fd_except, 0) <= 0)
        {
            if (!is_running())
                return 0;

            // Select failed.
            return manager->log(
                TCPServerManager::ERR,
                m_log.c_str(),
                "select() failed. {%d}",
                WSAGetLastError()
                );
        }

        if (FD_ISSET(m_slisten, &fd_read))
        {
            // Accept a connection.
            if ((sclient = accept(m_slisten, (sockaddr*)&addr, &addrlen)) == SOCKET_ERROR)
            {
                if (!is_running())
                    return 0;

                return manager->log(
                    TCPServerManager::ERR,
                    m_log.c_str(),
                    "Failed to accept a connection. {%d}",
                    WSAGetLastError()
                    );
            }

            // Set the timeout.
            setsockopt(sclient, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof timeout);
            ssl = NULL;

            // Attempt the handshake if SSL is enabled.
            if (m_ssl_ctx != NULL)
            {
                ssl = SSL_new(m_ssl_ctx);
                SSL_set_fd(ssl, sclient);
                err = SSL_accept(ssl);

                if (err <= 0)
                {
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                    ssl = NULL;
                }
            }

            // Add to the list of clients.
            ioctlsocket(sclient, FIONBIO, &mode);
            clients.push_back(TCPClient(sclient, ssl));

        }
        else if (FD_ISSET(m_slisten, &fd_except))
        {
            getsockopt(m_slisten, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
            return manager->log(
                TCPServerManager::ERR,
                m_log.c_str(),
                "Listening socket error. {%d}",
                err
                );
        }

        it = clients.begin();
        while (it != clients.end())
        {
            // Check the fd_sets for activity.
            if (FD_ISSET(it->socket, &fd_except))
            {
                FD_CLR(it->socket, &fd_except);
                goto close_connection;
            }
            else
            {
                if (FD_ISSET(it->socket, &fd_read))
                {
                    // Recv bytes.
                    recv_bytes = it->recv();

                    // Client closed the connection.
                    if (recv_bytes == 0)
                        goto close_connection;

                    if (recv_bytes == SOCKET_ERROR)
                    {
                        getsockopt(it->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
                        if (err != WSAEWOULDBLOCK) goto close_connection;
                    }
                    else
                    {
                        // Increment the recieved bytes and continue.
                        it->header_size += recv_bytes;
                        FD_CLR(it->socket, &fd_read);
                    }
                }
                else if (FD_ISSET(it->socket, &fd_write))
                {
                    // If the connection failed the handshake, don't send a response.
                    if (it->ssl == NULL && m_ssl_ctx != NULL)
                    {
                        FD_CLR(it->socket, &fd_write);
                        goto close_connection;
                    }

                    // Generate the content if there is none.
                    if (it->content == NULL)
                    {
                        HTTPRequest request(it->headers, it->header_size);
                        manager->log(
                            TCPServerManager::INFO,
                            m_log.c_str(),
                            "%s:%d %s %s %d",
                            #if defined(_MSC_VER)
                                inet_ntop(AF_INET, &(addr.sin_addr), ip_buffer, INET_ADDRSTRLEN)
                            #else
                                inet_ntoa(addr.sin_addr)
                            #endif
                            ,
                            ntohs(addr.sin_port),
                            request.get_method().c_str(),
                            request.get_uri().c_str(),
                            generate_response(&(*it), &request) // Generate the response.
                        );
                    }

                    // Send bytes.
                    send_bytes = it->send();

                    if (send_bytes == SOCKET_ERROR)
                    {
                        getsockopt(it->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
                        if (err != WSAEWOULDBLOCK) goto close_connection;
                    }

                    if (it->header_size == 0 && it->content_size == 0)
                    {
                        // Send complete.
                        FD_CLR(it->socket, &fd_write);
                        goto close_connection;
                    }

                }
            }

            it++;
            continue;

        // Close a connection.
        close_connection:
            it->close();
            clients.erase(it);
            it = clients.begin();
        }
    }

    // Close lingering clients.
    for (it = clients.begin(); it != clients.end(); it++)
        it->close();

    return 0;
}

/**
 * TCPServer::generate_response
 *
 * @description Starts the listening socket and dispatches the worker thread.
 */
status TCPServer::generate_response(TCPClient* client, HTTPRequest* request)
{
    map<string, string> m_params;
    TCPServerManager* manager;
    HTTPLocation *location;
    string path, response;
    size_t size;
    char *file;

    // Get the location from the request.
    location = m_uri_map.get_location(request);
    manager = TCPServerManager::get_manager();

    if (location == NULL)
    {
        // Generate a 404 response.
        file = m_uri_map.get_error("404", &size);

        if (file == NULL)
        {
            // Use the default 404 text.
            size = strlen(SPP_HTTP_404);
            file = new char[size];
            strcpy(file, SPP_HTTP_404);
        }

        client->header_size = sprintf(
            client->headers,
            "HTTP/1.1 404 Not Found\n \
            Server: Server++\n        \
            Content-Type: text/html\n \
            Content-Length: %d\n\r\n",
            size
        );

        client->content_size = size;
        client->content = file;
        return NOT_FOUND;
    }

    if (location->is_proxied())
    {
        // TODO
    }

    if (request->get_method() != "GET")
    {
        // TODO
    }

    // Serve the static file.
    path = location->get_path(request);
    file = read_file(path.c_str(), &size);

    if (file == NULL)
    {
        // Generate a 500 response.
        file = m_uri_map.get_error("500", &size);

        if (file == NULL)
        {
            // Use the default 500 text.
            size = strlen(SPP_HTTP_500);
            file = new char[size];
            strcpy(file, SPP_HTTP_500);
        }

        client->header_size = sprintf(
            client->headers,
            "HTTP/1.1 500 Internal Server Error\n \
            Server: Server++\n                    \
            Content-Type: text/html\n             \
            Content-Length: %d\n\r\n",
            size
        );

        client->content = file;
        client->content_size = size;
        return INTERNAL_SERVER_ERROR;
    }

    // Generate a 200 response.
    client->header_size = sprintf(
        client->headers,
        "HTTP/1.1 200 OK\n  \
         Server: Server++\n \
         Content-Type: %s\n \
         Content-Length: %d\n\r\n",
        manager->get_type(get_ext(path.c_str(), path.size())).c_str(),
        size
    );

    client->content = file;
    client->content_size = size;
    return OK;
}

/**
 * TCPServer::wait
 *
 * @description Wait indefinitely for the listening thread.
 */
void TCPServer::wait(void)
{
#if defined(SPP_WINDOWS)
    WaitForSingleObject(m_listener, INFINITE);
#endif
}

/**
 * TCPServer::stop
 *
 * @description Stop the server.
 */
void TCPServer::stop(void)
{
    m_mtx_stop.aquire();

    // Set the flag and close the listening socket.
    m_stop = true;
    closesocket(m_slisten);

    m_mtx_stop.release();
}

/**
 * TCPServerManager::add_server
 *
 * @description Adds a server to the collection.
 * @param {server} // The server to add.
 */
void TCPServerManager::add_server(TCPServer* server)
{
    m_servers.push_back(server);
}

/**
 * TCPServerManager::wait_for_servers
 *
 * @description Waits indefinitely for all servers.
 */
void TCPServerManager::wait_for_servers(void)
{
    std::list<TCPServer*>::iterator it;

    for (it = m_servers.begin(); it != m_servers.end(); it++)
        (*it)->wait();
}

/**
 * TCPServerManager::start_servers
 *
 * @description Starts all the servers in the collection.
 */
void TCPServerManager::start_servers(void)
{
    std::list<TCPServer*>::iterator it;

    for (it = m_servers.begin(); it != m_servers.end(); it++)
        (*it)->start();
}

/**
 * TCPServerManager::stop_servers
 *
 * @description Stops all the serves in the collection.
 */
void TCPServerManager::stop_servers(void)
{
    std::list<TCPServer*>::iterator it;

    for (it = m_servers.begin(); it != m_servers.end(); it++)
        (*it)->stop();
}

/**
 * TCPServerManager::add_type
 *
 * @description Adds a type to the server manager.
 */
void TCPServerManager::add_type(string ext, string mime)
{
    m_mimes[ext] = mime;
}

/**
 * TCPServerManager::get_type
 *
 * @description Returns a type from the provided extension.
 */
string TCPServerManager::get_type(string ext)
{
    return m_mimes[ext];
}