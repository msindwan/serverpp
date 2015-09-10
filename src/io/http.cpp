/**
 * Serverpp HTTP implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-10
 */

#include <spp\http.h>

using namespace spp;
using namespace std;

/**
 * HTTPRequest Constructor
 *
 * @description Parses the HTTP request and stores required information.
 * @param[out] {request} // The request string.
 * @param[out] {size}    // The size of the request.
 */
HTTPRequest::HTTPRequest(char* request, size_t size)
{
    char *key, *value, *next;
    string uri, query;
    int pos, cpos;

    cpos = 0;

    // Parse the request line. We don't need other headers at the moment.
    m_method = string(request, trim(&request, size, &cpos));
         uri = string(request, trim(&request, size, &cpos));
  m_protocol = string(request, trim(&request, size, &cpos));

  m_uri = uri;

  // Parse query string parameters.
  if ((pos = m_uri.find_first_of("?")) != string::npos)
  {
      m_uri = uri.substr(0, pos++);
      query = uri.substr(pos, uri.length());
     
      // Get key.
#if defined(WIN32) || defined(_WIN32_)
      key = strtok_s((char*)query.c_str(), "=", &next);
#else
      key = strtok((char*)query.c_str(), "=");
#endif
      while (key != NULL)
      {
          // Get value.
#if defined(WIN32) || defined(_WIN32_)
          value = strtok_s(NULL, "&", &next);
#else
          value = strtok(NULL, "&");
#endif
          if (value == NULL)
              break;

          // Store the key value pair.
          m_params[key] = value;
#if defined(WIN32) || defined(_WIN32_)
          key = strtok_s(NULL, "=", &next);
#else
          key = strtok(NULL, "=");
#endif
      }
  }
}

/**
 * HTTPLocation Constructor
 *
 * @description Creates a location for responses.
 * @param[out] {location} // The location configuration.
 * @param[out] {server}   // The server configuration.
 */
HTTPLocation::HTTPLocation(jToken* location, jToken* server)
{
    jToken* root_token;
    char* groot;

    m_aliased = false;
    m_proxy_pass = false;

    // Get root directory.
    root_token = jconf_get(server, "o", "root");

    if (root_token != NULL)
    {
        if (root_token->type != JCONF_STRING)
            throw HTTPException();

        groot = (char*)root_token->data;
        m_root = string(groot, jconf_strlen(groot, "\""));
    }
    
    // Construct the location based on the object type.
    switch (location->type)
    {
    case JCONF_STRING:
        // A global root is required.
        if (root_token == NULL)
            throw HTTPException();

        // The location is aliased.
        m_index = string((char*)location->data, jconf_strlen((char*)location->data, "\""));
        m_aliased = true;
        break;

    case JCONF_NULL:
        // A global root is required.
        if (root_token == NULL)
            throw HTTPException();

    case JCONF_OBJECT:
        // TODO: object and other configurations
        break;
    }
}

/**
 * HTTPLocation::get_path
 *
 * @description Returns the path to a resource on a GET request.
 * @param[out] {request}  // The request object to retrieve the path.
 */
string HTTPLocation::get_path(HTTPRequest* request)
{
    map<string, string> params;
    string path;

    params = request->get_params();
    path = m_aliased ? m_root + m_index : m_root + request->get_uri();
    render_template(path, &params);

    return path;
}

/**
 * HTTPLocation::get_path
 *
 * @description Returns the path to a resource with template parameters.
 * @param[out] {params}  // The substitution parameters.
 */
string HTTPLocation::get_path(map<string, string>* params)
{
    string path;

    path = m_root + m_index;
    render_template(path, params);

    return path;
}

/**
 * HTTPUriMap Destructor
 */
HTTPUriMap::~HTTPUriMap(void)
{
    list <pair <regex*, HTTPLocation*> >::iterator expr;
    list <HTTPLocation*>::iterator alias;
    list <HTTPLocation*> aliases;

    // Free expressions.
    for (expr = m_expressions.begin(); expr != m_expressions.end(); expr++)
    {
        delete expr->first;
        delete expr->second;
    }

    // Free errors.
    for (expr = m_errors.begin(); expr != m_errors.end(); expr++)
    {
        delete expr->first;
        delete expr->second;
    }

    // Free aliases.
    aliases = m_locations.get_values();
    for (alias = aliases.begin(); alias != aliases.end(); alias++)
        delete (*alias);
    
    m_expressions.clear();
    m_errors.clear();
}

/**
 * HTTPUriMap::set_location
 *
 * @description Assoicates a key with the provided location.
 * @param[out] {key}      // The location key.
 * @param[out] {location} // The location.
 */
void HTTPUriMap::set_location(const char* type, const char* key, HTTPLocation* location)
{
    regex* rgx;
    
    // Create a regex rule for locations.
    if (!strcmp(SPP_HTTP_REGEX, type))
    {
        rgx = new regex(key);
        m_expressions.push_back(make_pair(rgx, location));
    }
    // Create a regex rule for errors.
    else if (!strcmp(SPP_HTTP_ERROR, type))
    {
        rgx = new regex(key);
        m_errors.push_back(make_pair(rgx, location));
        // TODO: If not static file, throw error!
    }
    // Add the rule to the suffix tree.
    else if (!strcmp(SPP_HTTP_MAP, type))
    {
        m_locations.set(key, location);
    }
    else throw HTTPException();
}

/**
 * HTTPUriMap::get_error
 *
 * @description Retrieves the error location.
 * @param[out] {key}  // The location key.
 * @returns           // The location (NULL if not found).
 */
HTTPLocation* HTTPUriMap::get_error(const char* key)
{
    list< pair<regex*, HTTPLocation*> >::iterator it;

    // Compare against each regex.
    for (it = m_errors.begin(); it != m_errors.end(); it++)
    {
        if (regex_match(key, *it->first))
            return it->second;
    }
    
    return NULL;
}

/**
 * HTTPUriMap::get_location
 * 
 * @description Retrieves the location.
 * @param[out] {key}  // The location key.
 * @returns           // The location (NULL if not found).
 */
HTTPLocation* HTTPUriMap::get_location(HTTPRequest* request)
{
    list< pair<regex*, HTTPLocation*> >::iterator it;
    HTTPLocation* location;
    string uri;

    // First do a direct string comparision.
    uri = request->get_uri();

    // If not found, compare each regex.
    if ((location = m_locations.get(uri.c_str())) == NULL)
    {
        for (it = m_expressions.begin(); it != m_expressions.end(); it++)
        {
            if (regex_match(uri, *it->first))
                return it->second;
        }
    }

    return location;
}