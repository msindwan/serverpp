/**
 * Serverpp log implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#include <spp/log.h>

using namespace spp;

// Static logger level string array.
static const char* levels[] =
{
    "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
};

/**
 * Logger::log
 *
 * @description Logs a message to the provided stream.
 * @param[out] {level}   // The logger level.
 * @param[out] {file}    // The file stream.
 * @param[out] {message} // The message.
 */
void Logger::log(Level level, FILE* file, const char* message, ...)
{
    struct tm timeinfo;
    char date[100];
    time_t ctime;
    va_list args;

    m_mtx.aquire();

    // Format the time.
    time(&ctime);
#if defined(SPP_WINDOWS)
    localtime_s(&timeinfo, &ctime);
#elif defined(__unix__)
    timeinfo = *localtime(&ctime);
#endif

    strftime(date, sizeof(date), "%c", &timeinfo);
    fprintf(
        file,
        "%6s [%s] : ",
        levels[level],
        date
        );

    // Print the message.
    va_start(args, message);
    vfprintf(file, message, args);
    fprintf(file, "\n");
    va_end(args);

    m_mtx.release();
}

/**
 * Logger::log
 *
 * @description Logs a message to a file.
 * @param[out] {level}   // The logger level.
 * @param[out] {file}    // The path to the output file.
 * @param[out] {message} // The message.
 * @returns // True if the stream was opened; false otherwise.
 */
bool Logger::log(Level level, const char* file, const char* message, ...)
{
    struct tm timeinfo;
    char date[100];
    time_t ctime;
    va_list args;
    FILE* stream;

    m_mtx.aquire();

    // Attempt to open the file.
    stream = NULL;
#if defined(SPP_WINDOWS)
    fopen_s(&stream, file, "ab");
#else
    stream = fopen(file, "ab");
#endif

    if (stream == NULL)
        return false;

    // Format the time.
    time(&ctime);
#if defined(SPP_WINDOWS)
    localtime_s(&timeinfo, &ctime);
#elif defined(__unix__)
    timeinfo = *localtime(&ctime);
#endif

    strftime(date, sizeof(date), "%c", &timeinfo);
    fprintf(
        stream,
        "%6s [%s] : ",
        levels[level],
        date
        );

    // Print the message.
    va_start(args, message);
    vfprintf(stream, message, args);
    fprintf(stream, "\n");
    va_end(args);

    fclose(stream);
    m_mtx.release();

    return true;
}