/**
 * Serverpp HTTP
 *
 * Description: Includes definitions for well-defined HTTP components.
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#ifndef __HTTP_SPP_H__
#define __HTTP_SPP_H__

#include "jconf\parser.h"
#include "collection.h"
#include <string.h>
#include "util.h"
#include <string>
#include <regex>

// Location directive keywords.
#define SPP_HTTP_REGEX "regex"
#define SPP_HTTP_ERROR "error"
#define SPP_HTTP_MAP   "map"

// Constant default response content.
#define SPP_HTTP_500 "<html><body><h2>Server++</h2><div>500 Internal Server Error</div></body></html>"
#define SPP_HTTP_404 "<html><body><h2>Server++</h2><div>404 Not Found</div></body></html>"

namespace spp
{
    enum status
    {
        OK                    = 200,
        FOUND                 = 302,
        FORBIDDEN             = 403,
        NOT_FOUND             = 404,
        INTERNAL_SERVER_ERROR = 500
    };

    /**
     * HTTPException
     */
    class HTTPException { };

    /**
     * HTTPRequest: An object representing an HTTP request that
     * stores relevant parsed data.
     */
    class HTTPRequest
    {
    public:
        // Constructor.
        HTTPRequest(char*, size_t);

    public:
        // Getters and setters.
        std::map<std::string, std::string> get_params(void) { return m_params; }
        std::string get_method() { return m_method; }
        std::string get_protocol() { return m_protocol; }
        std::string get_uri() { return m_uri; }

    private:
        // Data members.
        std::map<std::string, std::string> m_params;
        std::string m_protocol,
                    m_method,
                    m_uri;
    };

    /**
     * HTTPLocation: An object that defines a location directive
     * for get requests.
     */
    class HTTPLocation
    {
    public:
        // Constructor.
        HTTPLocation(jToken*, jToken*);

    public:
        // Getters and Setters.
        std::string get_path(std::map<std::string, std::string>*);
        std::string get_path(HTTPRequest*);

        bool is_proxied() { return m_proxy_pass; }

    private:
        // Data members.
        std::string m_root;
        std::string m_index;
        bool m_proxy_pass;
        bool m_aliased;
    };

    /**
     * HTTPUriMap: A collection of HTTPLocations mapped to uri constants
     * and or regular expressions.
     */
    class HTTPUriMap
    {
    public:
        // Constructor.
        HTTPUriMap(void) {};

    public:
        // Getters and Setters.
        void set_location(const char*, const char*, HTTPLocation*);
        HTTPLocation* get_location(HTTPRequest*);
        char* get_error(const char*, size_t*);

    private:
        // Data members.
        std::list< std::pair<std::regex, HTTPLocation*> > m_expressions;
        std::list< std::pair<std::regex, HTTPLocation*> > m_errors;
        SuffixTree<HTTPLocation*> m_locations;
    };
}

#endif