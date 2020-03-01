#pragma once

#include <vector>
#include <map>
#include <string>

#include "network_interface.h"

class HTTPClient : public INetworkClient
{
    typedef void CURL;    
public:
    struct SInitSettings {
        SInitSettings()
            : secureConnections(false)
        {}
        bool secureConnections;
    };

    HTTPClient( INetworkEntity::TConnectionId _id );
    ~HTTPClient();
    bool init( SInitSettings _settings );

    virtual PEnvironmentRequest getRequestInstance() override;

    bool sendRequestGET( const std::string & _url, const bool _fetchCookies = false );
    bool sendRequestPOST( const std::string & _url, const std::string & _data, const bool _setCookies = false );

    // check
    bool checkServerAlive( const std::string & _url );

    // http
    void setHTTPHeadersOnce( const std::vector<std::string> & _headers );
    const std::map<std::string,std::string> & getCookieTokens();
    void clearCookies();

    std::string * receiveAnswer();
    uint64_t getResponseSize();


private:
    void fetchCookies();

    // data
    SInitSettings m_settings;
    std::string m_resultStr;
    std::map<std::string,std::string> m_cookies;

    // service
    CURL * m_curlSession;
    struct curl_slist * m_httpHeaders;
};
using PHTTPClient = std::shared_ptr<HTTPClient>;

