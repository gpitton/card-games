#pragma once

// Common web server utilities, mostly taken from Boost Beast
// HTTP server, synchronous example.

//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "mcengine.hh"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <cstdlib>
//#include <format>  // enable after updating GCC
#include <iostream>
#include <memory>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>


//------------------------------------------------------------------------------

// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path)
{
    using beast::iequals;
    const auto ext = [&path]
    {
        const auto pos = path.rfind(".");
        if(pos == beast::string_view::npos)
        {
            return beast::string_view{};
        }
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    beast::string_view base,
    beast::string_view path)
{
	if (base.empty())
    {
		return std::string(path);
    }
    std::string result(base);
#ifdef BOOST_MSVC
    constexpr char path_separator = '\\';
    if(result.back() == path_separator)
    {
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
    for(char& c : result)
    {
        if(c == '/')
        {
            c = path_separator;
        }
    }
#else
    constexpr char path_separator = '/';
    if(result.back() == path_separator)
    {
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
#endif
    return result;
}

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template <class Body, class Allocator>
http::message_generator
handle_request(
    beast::string_view doc_root,
    beast::string_view file_path,
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    // Returns a bad request response
    const auto bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    const auto not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    const auto server_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if( req.method() != http::verb::get &&
        req.method() != http::verb::head &&
        req.method() != http::verb::post)
    {
        return bad_request("Unknown HTTP-method");
    }

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos)
    {
        return bad_request("Illegal request-target");
    }

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if(req.target().back() == '/')
    {
        path.append("index.html");
    }

    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    //body.open(path.c_str(), beast::file_mode::scan, ec);
    // We return the main page for any GET request.
    body.open(file_path.data(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if(ec == beast::errc::no_such_file_or_directory)
    {
        return not_found(req.target());
    }

    // Handle an unknown error
    if(ec)
    {
        return server_error(ec.message());
    }

    // Cache the size since we need it after the move
    const std::uint64_t size = body.size();

    switch (req.method())
    {
        // Respond to HEAD request
        case http::verb::head:
        {
            http::response<http::empty_body> res{http::status::ok, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, mime_type(path));
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            return res;
        }

        // Respond to GET request
        case http::verb::get:
        {
            http::response<http::file_body> res{
                std::piecewise_construct,
                std::make_tuple(std::move(body)),
                std::make_tuple(http::status::ok, req.version())};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, mime_type(path));
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            return res;
        }

        // Respond to POST request
        case http::verb::post:
        {
            GameState game = GameState::fromJson(req);
            MonteCarloEngine engine;
            double ps[3];
            engine.run(game, ps);
            // TODO enable after updating GCC
            /*const std::string ps_str = std::format(
                "{\"ps\":[{},{},{}]}", ps[0], ps[1], ps[2]);
            */
            const std::string ps_str = "{\"ps\":[" + std::to_string(ps[0])
                + ',' + std::to_string(ps[1]) + ',' + std::to_string(ps[2]) + "]}";
            http::response<http::string_body> res{
                http::status::ok,
                req.version(),
                ps_str  // Forwarded to body constructor.
                };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "application/json");
            res.content_length(ps_str.size());
            res.keep_alive(req.keep_alive());
            return res;
        }
    }
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, const char * const what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

