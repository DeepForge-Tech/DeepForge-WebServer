//
// Copyright (C) 2020 Maciej Sobczak
//
// This file declares the interface of the minimal, embedded HTTP server.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file Boost_Software_License_1_0.txt
// or copy at http://www.opensource.org/licenses/bsl1.0.html)
//

#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED

#include <ostream>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

/// Namespace with scope for all Embedded HTTP Server definitions.
namespace http
{

/// Bit masks for various categories of diagnostic log messages.
const unsigned int log_connections       = 0x01;
const unsigned int log_static_requests   = 0x02;
const unsigned int log_static_responses  = 0x04;
const unsigned int log_dynamic_requests  = 0x08;
const unsigned int log_dynamic_responses = 0x10;
const unsigned int log_everything        = 0x1f;

/// \brief Start the embedded HTTP server.
///
/// Start the singleton embedded HTTP server.
/// The server creates the listener socket
/// and operates it in the context of the calling thread.
///
/// @param port_number port number for the listening socket
/// @param base_directory directory containing static dist.
/// @param error_log optional output stream for diagnostic logs
/// @param log_events_mask bit mask for selecting active categories of log messages
/// @return This function does not return as long as the server functions properly.
void server_start(int port_number, const char * base_directory);
void server_start(int port_number, const char * base_directory,
    std::ostream & error_log, unsigned int log_events_mask = log_everything);

/// Type defining possible connection events, used to notify the connection callback.
enum connection_event
{
    just_connected, ///< The given stream (client connection) was just created.
    to_be_closed    ///< The given stream is to be closed and destroyed.
};

/// Type of function callback for connection event notifications.
/// @param out stream object that is the subject of the current event.
/// @param event the name of current event.
typedef std::function<void(std::ostream &, connection_event)>
    connection_callback_type;

/// Register the connection callback.
///
/// Register the connection callback. The callback function will be notified
/// whenever the client connection is created to is about to be destroyed.
/// The previous callback registration (if any) is replaced.
///
/// @param callback the callback function to register.
void register_connection_callback(connection_callback_type callback);

/// Type of function callback for handling GET requests.
/// @param out stream object handling the requesting client connection
/// @param path name of the requested resource (up to the '?' sign, if any)
/// @param params the URL parameters (from the '?' sign to the end of URL)
typedef std::function<void(std::ostream &, const std::string &, const std::string &)>
    get_action_type;

/// Register generic GET handler.
///
/// Register generic GET handler.
/// The generic handler is responsible for producing the whole response,
/// including HTTP headers, and for flushing the output stream.
///
/// Note: generic actions are called in the context of the thread
/// that is dedicated for the given connection and the stream object
/// retains state between invocations (it directly represents the connection stream).
///
/// @param name name of the "resource" to be handled by the callback.
/// @param f function callback that will handle the GET request.
void register_generic_get_action(const char * name, get_action_type f);

/// Register text/html GET handler.
///
/// Register text/html GET handler.
/// The text/html handler is responsible for producing only the response data,
/// the HTTP headers are taken care of automatically.
///
/// Note: the text/html action is called in the context of the thread
/// that is dedicated for the given connection, but the stream object
/// is temporary and does not retain state between invocations.
/// The collected output is automatically flushed to the actual connection stream.
///
/// @param name name of the "resource" to be handled by the callback.
/// @param f function callback that will handle the GET request.
void register_html_get_action(const char * name, get_action_type f);

/// Register text/plain GET handler.
///
/// Register text/plain GET handler.
/// The text/plain handler is responsible for producing only the response data,
/// the HTTP headers are taken care of automatically.
///
/// Note: the text/plain action is called in the context of the thread
/// that is dedicated for the given connection, but the stream object
/// is temporary and does not retain state between invocations.
/// The collected output is automatically flushed to the actual connection stream.
///
/// @param name name of the "resource" to be handled by the callback.
/// @param f function callback that will handle the GET request.
void register_text_get_action(const char * name, get_action_type f);

/// Type of function callback for handling POST requests.
/// @param out stream object handling the output part of the requesting client connection
/// @param path name of the requested resource (up to the '?' sign, if any)
/// @param params the URL parameters (from the '?' sign to the end of URL)
/// @param in stream object handling the input part of the requesting connection
/// @param content_length number of bytes to be consumed from the in stream
/// @param mime_type MIME type declared for the POST request by the client
typedef std::function<void(std::ostream &, const std::string &, const std::string &,
    std::istream &, std::size_t, const std::string &)>
    post_action_type;

/// Register generic POST handler.
///
/// Register generic POST handler.
/// The generic handler is responsible for producing the whole response,
/// including HTTP headers.
///
/// Note: generic actions are called in the context of the thread
/// that is dedicated for the given connection and the stream object
/// retains state between invocations (it directly represents the connection stream).
///
/// @param name name of the "resource" to be handled by the callback.
/// @param f function callback that will handle the POST request.
void register_generic_post_action(const char * name, post_action_type f);

/// Register text/html POST handler.
///
/// Register text/html POST handler.
/// The text/html handler is responsible for producing only the response data,
/// the HTTP headers are taken care of automatically.
///
/// Note: the text/html action is called in the context of the thread
/// that is dedicated for the given connection, but the stream object
/// is temporary and does not retain state between invocations.
/// The collected output is automatically flushed to the actual connection stream.
///
/// @param name name of the "resource" to be handled by the callback.
/// @param f function callback that will handle the POST request.
void register_html_post_action(const char * name, post_action_type f);

/// Register text/plain POST handler.
///
/// Register text/plain POST handler.
/// The text/plain handler is responsible for producing only the response data,
/// the HTTP headers are taken care of automatically.
///
/// Note: the text/plain action is called in the context of the thread
/// that is dedicated for the given connection, but the stream object
/// is temporary and does not retain state between invocations.
/// The collected output is automatically flushed to the actual connection stream.
///
/// @param name name of the "resource" to be handled by the callback.
/// @param f function callback that will handle the POST request.
void register_text_post_action(const char * name, post_action_type f);

/// Encode basic HTML entities.
///
/// Encode basic HTML entities - '<', '>', '&'.
/// Those special characters are replaced with "&lt;", "&gt;" and "&amp;".
/// @param s string to be encoded.
/// @return encoded string.
std::string html_encode(const std::string & s);

/// Encode string for safe use within URL.
///
/// Encode string for safe use within URL.
/// Alpha-numeric characters, with '-', '_', '.' and '~' are left unchanged,
/// other characters are encoded as %hh hex code (space is replaced with '+').
/// @param s string to be encoded.
/// @return encoded string.
std::string url_encode(const std::string & s);

/// Decode part of the URL (used for parameters).
///
/// Decode part of the URL, using reverse logic of url_encode.
/// @param s string to be decoded.
/// @return decoded string.
std::string url_decode(const std::string & s);

/// Type of map {key->value,...} for decoding URL and form parameters.
typedef std::unordered_map<std::string, std::string> params_map_type;

/// Decode URL or form parameters.
///
/// Decode URL or form parameters into map. The encoded parameters
/// are expected to have the "key1=value1&key2=value2&..." format.
/// @param params parameters, stored as a single string, to be decoded.
/// @return decoded parameter map.
params_map_type decode_params(const std::string & params, bool decode);
params_map_type decode_params(const std::vector<char> & params, bool decode);

/// Generate basic HTTP header.
///
/// Generate basic HTTP header, typically for the generic resource handler,
/// of the form:
/// HTTP/1.1 200 OK
/// Content-Type: mime_type
/// Content-Length: content_length
/// Cache-Control: ...
///
/// @param mime_type MIME type declaration.
/// @param content_length number of bytes in the response body.
/// @param cache whether the given response is supposed to be cached
/// @return generated HTTP header.
std::string header(const std::string & mime_type,
    std::size_t content_length = 0, bool cache = false);

} // namespace http

#endif // HTTP_SERVER_H_INCLUDED
