#include "common.hh"
#include <boost/beast.hpp>
#include <iostream>
#include <thread>

namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;

// File to serve. We only serve this file for all GET requests.
constexpr const char* g_path = "briscola.html";


// Handles an HTTP server connection
void
do_session(
    tcp::socket& socket,
    const std::shared_ptr<const std::string>& doc_root)
{
    beast::error_code ec;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    for(;;)
    {
        // Read a request
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);
        if(ec == http::error::end_of_stream)
        {
            break;
        }
        if(ec)
        {
            return fail(ec, "read");
        }

        // Handle request
        http::message_generator msg =
            handle_request(*doc_root, g_path, std::move(req));

        // Determine if we should close the connection
        bool keep_alive = msg.keep_alive();

        // Send the response
        beast::write(socket, std::move(msg), ec);

        if(ec)
        {
            return fail(ec, "write");
        }
        if(! keep_alive)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            break;
        }
    }

    // Send a TCP shutdown
    socket.shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    try
    {
        // Check command line arguments.
        if (argc != 4)
        {
            std::cerr <<
                "Usage: server <address> <port> <doc_root>\n" <<
                "Example:\n" <<
                "    server 0.0.0.0 8080 .\n";
            return EXIT_FAILURE;
        }
        const auto address = net::ip::make_address(argv[1]);
        const auto port = static_cast<unsigned short>(std::atoi(argv[2]));
        const auto doc_root = std::make_shared<std::string>(argv[3]);

        // The io_context is required for all I/O
        net::io_context ioc{1};  // How many threads to run concurrently

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};
        for(;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            // Launch the session, transferring ownership of the socket
            std::thread{std::bind(
                &do_session,
                std::move(socket),
                doc_root)}.detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

