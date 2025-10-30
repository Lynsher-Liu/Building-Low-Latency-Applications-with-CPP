#pragma once

#include <boost/asio/ssl.hpp>
#include <string>

namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>

namespace detail {

inline
void
load_root_certificates(ssl::context& ctx, boost::system::error_code& ec)
{
    std::string cert = "";
}
}

// Load the root certificates into an ssl::context

inline
void
load_root_certificates(ssl::context& ctx, boost::system::error_code& ec)
{
    detail::load_root_certificates(ctx, ec);
}

inline
void
load_root_certificates(ssl::context& ctx)
{
    boost::system::error_code ec;
    detail::load_root_certificates(ctx, ec);
    if(ec)
        throw boost::system::system_error{ec};
}
