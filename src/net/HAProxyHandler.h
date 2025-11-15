#pragma once
#include <stdint.h>
#include <vector>
#include <boost/system/error_code.hpp>
#include "NetTypes.h"

// Maximum size of the HAProxy header, per documentation:
#define HAPROXY_HEADER_MAX 107
// Minimum size of the HAProxy header:
#define HAPROXY_HEADER_MIN 16

class HAProxyHandler
{
    private:
        // Data members.
        std::vector<uint8_t> m_data_buf;
        std::vector<uint8_t> m_tlv_buf;
        
        NetAddress m_local;
        NetAddress m_remote;

        bool m_is_local = false;

        bool m_has_error = false;
        boost::system::error_code m_error_code;

        void set_error(const boost::system::error_code &ec);
        size_t parse_proxy_block();

        size_t parse_v1_block();
        size_t parse_v2_block();
    public:
        size_t consume(const uint8_t* buffer, size_t length); 
        
        inline NetAddress get_local() const
        {
            return m_local;
        }

        inline NetAddress get_remote() const
        {
            return m_remote;
        }

        inline const std::vector<uint8_t>& get_tlvs() const
        {
            return m_tlv_buf;
        }

        inline bool has_tlvs() const
        {
            return m_tlv_buf.size() > 0;
        }

        inline bool is_local() const
        {
            return m_is_local;
        }

        inline bool has_error() const
        {
            return m_has_error;
        }

        inline boost::system::error_code get_error() const
        {
            return m_error_code;
        }
};
