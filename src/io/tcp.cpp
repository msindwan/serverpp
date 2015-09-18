/**
 * Serverpp TCP Implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-17
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

    if (header_size > 0)
    {
        sent_bytes = ssl ?
            SSL_write(ssl, (char*)headers, header_size) :
            ::send(socket, headers, header_size, 0);

        header_size -= sent_bytes;
    }

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

    string slog,
           type,
           key;

    FILE* log;
    int i, rtn;

    jToken *location_cert,
           *location_key,
           *location,
           *ssl_token,
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

    // Set SSL context.
    temp = jconf_get(server, "o", "ssl");

    if (temp != NULL)
    {
        ssl_token = jconf_get(temp, "o", "enabled");

        if (ssl_token == NULL || ssl_token->type == JCONF_TRUE)
        {
            location_cert = jconf_get(temp, "o", "cert");

            if (location_cert == NULL || location_cert->type != JCONF_STRING)
                throw TCPException();

            m_cert = string((char*)location_cert->data, jconf_strlen((char*)location_cert->data, "\""));

            location_key = jconf_get(temp, "o", "key");

            if (location_key == NULL || location_key->type != JCONF_STRING)
                throw TCPException();

            m_ckey = string((char*)location_key->data, jconf_strlen((char*)location_key->data, "\""));

            m_ssl_ctx = SSL_CTX_new(SSLv23_server_method());
            SSL_CTX_set_options(m_ssl_ctx, SSL_OP_SINGLE_DH_USE);

            if ((rtn = load_certificates(m_ssl_ctx, m_cert.c_str(), m_ckey.c_str())) < 0)
                throw TCPException();
        }
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
        m_locations.push_back(http_location);
        m_uri_map.set_location(type.c_str(), key.c_str(), http_location);
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
}

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
    u_long mode;
    errno_t err;
    SSL* ssl;

    int errlen, recv_bytes, send_bytes;
    char ip_buffer[INET_ADDRSTRLEN];

    manager = TCPServerManager::get_manager();
    addrlen = sizeof(addr);
    errlen = sizeof(err);
    timeout = 1000;
    mode = 1;

    while (is_running())
    {
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
            if ((sclient = accept(m_slisten, (sockaddr*)&addr, &addrlen)) == INVALID_SOCKET)
            {
                return manager->log(
                    TCPServerManager::ERR,
                    m_log.c_str(),
                    "Failed to accept a connection. {%d}",
                    WSAGetLastError()
                );
            }

            setsockopt(sclient, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof timeout);
            ssl = NULL;

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
                    recv_bytes = it->recv();

                    if (recv_bytes == 0)
                        goto close_connection;

                    if (recv_bytes == SOCKET_ERROR)
                    {
                        getsockopt(it->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
                        if (err != WSAEWOULDBLOCK) goto close_connection;
                    }
                    else
                    {
                        it->header_size += recv_bytes;
                        FD_CLR(it->socket, &fd_read);
                    }
                }
                else if (FD_ISSET(it->socket, &fd_write))
                {
                    if (it->ssl == NULL && m_ssl_ctx != NULL)
                    {
                        FD_CLR(it->socket, &fd_write);
                        goto close_connection;
                    }

                    if (it->content == NULL)
                    {
                        HTTPRequest request(it->headers, it->header_size);
                        generate_resource(&(*it), &request);

                        manager->log(
                            TCPServerManager::INFO,
                            m_log.c_str(),
                            "%s:%d %s %s",
                            inet_ntop(AF_INET, &(addr.sin_addr), ip_buffer, INET_ADDRSTRLEN),
                            ntohs(addr.sin_port),
                            request.get_method().c_str(),
                            request.get_uri().c_str());
                    }

                    send_bytes = it->send();

                    if (send_bytes == SOCKET_ERROR)
                    {
                        getsockopt(it->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
                        if (err != WSAEWOULDBLOCK) goto close_connection;
                    }

                    if (it->header_size == 0 && it->content_size == 0)
                    {
                        FD_CLR(it->socket, &fd_write);
                        goto close_connection;
                    }

                }
            }

            it++;
            continue;

        close_connection:
            it->close();
            clients.erase(it);
            it = clients.begin();
        }
    }

    for (it = clients.begin(); it != clients.end(); it++)
        it->close();

    return 0;
}

/**
 * TCPServer is_running.
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

void TCPServer::start(void)
{
    struct sockaddr_in addr;
    u_long mode;

    mode = 0;
    if (is_running())
        return;

    m_slisten = socket(AF_INET, SOCK_STREAM, 0);

    if (m_slisten == INVALID_SOCKET)
    {
        throw TCPException();
    }

    ioctlsocket(m_slisten, FIONBIO, &mode);

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

#if defined(SPP_WINDOWS)
    m_listener = (HANDLE)_beginthreadex(NULL, 0, &tcp_listener, this, 0, NULL);
#endif

    m_stop = false;
}

int TCPServer::generate_resource(TCPClient* client, HTTPRequest* request)
{
    HTTPLocation *location;
    map<string, string> m_params;
    TCPServerManager* manager;
    string path, response;
    char *file, *ext;
    size_t size;

    location = m_uri_map.get_location(request);
    manager = TCPServerManager::get_manager();
    file = NULL;

    if (location == NULL)
    {
        file = m_uri_map.get_error("404", &size);

        if (file == NULL)
        {
            file = new char[SPP_HTTP_404_LEN];
            strcpy(file, SPP_HTTP_404);
            size = SPP_HTTP_404_LEN;
        }

        client->header_size = sprintf(
            client->headers,
            "HTTP/1.1 404 Not Found\n \
            Server: Server++\n        \
            Content-Type: text/html\n \
            Content-Length: %d\n\r\n",
            size
        );

        client->code = NOT_FOUND;
        client->content_size = size;
        client->content = file;

        return 0;
    }

    if (location->is_proxied())
    {
        // TODO: 307 Temporary Redirect
        return 0;
    }

    if (request->get_method() != "GET")
    {
        // Method not allowed
    }

    path = location->get_path(request);
    file = read_file(path.c_str(), &size);

    if (file == NULL)
    {
        file = new char[SPP_HTTP_500_LEN];
        strcpy(file, SPP_HTTP_500);
        size = SPP_HTTP_500_LEN;

        client->header_size = sprintf(
            client->headers,
            "HTTP/1.1 500 Internal Server Error\n \
                    Server: Server++\n                   \
                            Content-Type: text/html\n            \
                                    Content-Length: %d\n\r\n",
                                    size
                                    );

        client->code = INTERNAL_SERVER_ERROR;
        client->content = file;
        client->content_size = size;

        return 0;
    }

    ext = get_ext(path.c_str(), path.size());

    client->header_size = sprintf(
        client->headers,
        "HTTP/1.1 200 OK\n \
                Server: Server++\n \
                        Content-Type: %s\n \
                                Content-Length: %d\n\r\n",
                                manager->get_type(ext).c_str(),
                                size
                                );

    client->content = file;
    client->content_size = size;


    return 0;
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