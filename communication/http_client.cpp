
#include <sys/time.h>
#include <thread>

#include <curl/curl.h>

#include "system/logger.h"
#include "http_client.h"

using namespace std;
//#define DEBUG_CURL

static constexpr const char * PRINT_HEADER = "HTTPClient:";

// ----------------------------------------------
// writer - функция обратного вызова. вызывается, когда данные по конкретной операции получены
// ----------------------------------------------
static size_t writer( char * fromWebServer, size_t blockSize, size_t blockCount, void * localContainer ){

    std::string  & result = *( std::string  * )localContainer;
    int offset = result.size();
    result.resize( offset + blockCount );

    // считывание по блокам
    for ( size_t blockNumber = 0 ; blockNumber < blockCount ; blockNumber++ ){
        result[ offset + blockNumber ] = fromWebServer[ blockNumber ];
    }

    return blockSize * blockCount;
}

// -----------------------------------------------------------------------------
// request override
// -----------------------------------------------------------------------------
class HttpClientRequest : public AEnvironmentRequest {

public:
    virtual void setOutcomingMessage( const std::string & _msg ) override {

        const bool rt = httpClient->sendRequestGET( _msg );
        if( ! rt ){
            VS_LOG_ERROR << PRINT_HEADER << " GET request failed" << endl;
        }

        AEnvironmentRequest::m_incomingMessage = ( * httpClient->receiveAnswer() );
    }

    virtual void setOutcomingMessage( const char * _bytes, int _bytesLen ) override {
    }

    virtual void setUserData( void * _data ) override {
    }

    HTTPClient * httpClient;
};
using PHttpClientRequest = std::shared_ptr<HttpClientRequest>;

// -----------------------------------------------------------------------------
// http client
// -----------------------------------------------------------------------------
HTTPClient::HTTPClient( INetworkEntity::TConnectionId _id )
    : INetworkClient(_id)
    , m_httpHeaders(nullptr)
{

    setlocale( LC_NUMERIC, "C" );

    m_curlSession = curl_easy_init();

    if( ! m_curlSession){
       VS_LOG_ERROR << PRINT_HEADER << " couldn't init CURL" << endl;
       throw;
    }

    curl_easy_setopt( m_curlSession, CURLOPT_NOSIGNAL, 1 ); // на Астре CURL кидает сигнал
    curl_easy_setopt( m_curlSession, CURLOPT_WRITEFUNCTION, writer );
    // установка указателя контекста (в функции обратного вызова это последний аргумент)
    curl_easy_setopt( m_curlSession, CURLOPT_WRITEDATA, (void*) & m_resultStr );//массив символов полученных в результате http-запроса
}

HTTPClient::~HTTPClient(){

    curl_slist_free_all( m_httpHeaders );
    curl_easy_cleanup( m_curlSession );
}

PEnvironmentRequest HTTPClient::getRequestInstance(){

    PHttpClientRequest request = std::make_shared<HttpClientRequest>();
    request->httpClient = this;

    return request;
}

bool HTTPClient::init( SInitSettings _settings ){

    m_settings = _settings;




    return true;
}

void HTTPClient::setHTTPHeadersOnce( const vector<string> & _headers ){

    // "Content-Type: text/xml; charset=utf-8"

    for( const string & header : _headers ){
        m_httpHeaders = curl_slist_append( m_httpHeaders, header.c_str() );
    }
}

bool HTTPClient::sendRequestGET( const std::string & _url, const bool _fetchCookies ){

    m_resultStr.clear();

    char errorBuffer[CURL_ERROR_SIZE] = "";
    long ferror = 0;

    //url-строка, посылаемая на сервер
    curl_easy_setopt( m_curlSession, CURLOPT_HTTPGET, 1 ); // после POST запроса Curl нужно обратно возвращать в GET режим
    curl_easy_setopt( m_curlSession, CURLOPT_COOKIEFILE, "" );
    curl_easy_setopt( m_curlSession, CURLOPT_URL, _url.c_str() );
    curl_easy_setopt( m_curlSession, CURLOPT_ERRORBUFFER, errorBuffer );
    curl_easy_setopt( m_curlSession, CURLOPT_FAILONERROR, & ferror );
    const long secureFlag = ( m_settings.secureConnections ? 1L : 0L );
    curl_easy_setopt( m_curlSession, CURLOPT_SSL_VERIFYPEER, secureFlag );
    curl_easy_setopt( m_curlSession, CURLOPT_SSL_VERIFYHOST, secureFlag );

#ifdef DEBUG_CURL
    curl_easy_setopt( m_curl, CURLOPT_VERBOSE, 1L );
#endif

    CURLcode curlCodeResult = curl_easy_perform( m_curlSession );

    long httpCode = 0;
    curl_easy_getinfo( m_curlSession, CURLINFO_RESPONSE_CODE, httpCode );

    if( curlCodeResult != CURLE_OK ){

        VS_LOG_ERROR << PRINT_HEADER << " Curl HTTP-Code = " << httpCode << endl;
        VS_LOG_ERROR << PRINT_HEADER << " curl - " << curl_easy_strerror(curlCodeResult) << endl;
        VS_LOG_ERROR << PRINT_HEADER << " error buffer - " << errorBuffer << " % ferror - " << ferror << endl;
        return false;
    }

    if( _fetchCookies ){
        fetchCookies();
    }

    return true;
}

bool HTTPClient::sendRequestPOST( const std::string & _url, const std::string & _data, const bool _setCookies ){

    // TODO если заголовок не указан явно, ставится по умолчанию xml содержимое
    if( ! m_httpHeaders ){
        setHTTPHeadersOnce( {"Content-Type: text/xml; charset=utf-8"} );
    }

    m_resultStr.clear();

    char errorBuffer[CURL_ERROR_SIZE] = "";
    long ferror = 0;

    //url-строка, посылаемая на сервер
    curl_easy_setopt( m_curlSession, CURLOPT_POST, 1 );
    curl_easy_setopt( m_curlSession, CURLOPT_COOKIEFILE, "" );
    curl_easy_setopt( m_curlSession, CURLOPT_URL, _url.c_str() );
    curl_easy_setopt( m_curlSession, CURLOPT_ERRORBUFFER, errorBuffer );
    curl_easy_setopt( m_curlSession, CURLOPT_FAILONERROR, & ferror );
    curl_easy_setopt( m_curlSession, CURLOPT_POSTFIELDS, _data.c_str() );
    curl_easy_setopt( m_curlSession, CURLOPT_POSTFIELDSIZE, _data.length() );

#ifdef DEBUG_CURL
    curl_easy_setopt( m_curl, CURLOPT_VERBOSE, 1L );
#endif

    if( m_httpHeaders ){
        curl_easy_setopt( m_curlSession, CURLOPT_HTTPHEADER, m_httpHeaders );
    }

    // cookie
    string cookies;
    if( _setCookies ){

        for( auto & cookie : m_cookies ){
            cookies += cookie.first + "=" + cookie.second + ";";
        }

        curl_easy_setopt( m_curlSession, CURLOPT_COOKIE, cookies.c_str() );
    }

    // run
    CURLcode curlCodeResult = curl_easy_perform( m_curlSession );

    long httpCode = 0;
    curl_easy_getinfo( m_curlSession, CURLINFO_RESPONSE_CODE, httpCode );

    if( curlCodeResult != CURLE_OK ){

        VS_LOG_ERROR << PRINT_HEADER << " Curl HTTP-Code = " << httpCode << endl;
        VS_LOG_ERROR << PRINT_HEADER << " curl - " << curl_easy_strerror( curlCodeResult ) << endl;
        VS_LOG_ERROR << PRINT_HEADER << " error buffer - " << errorBuffer << " % ferror - " << ferror;

        // remove headers
        if( m_httpHeaders ){
            curl_slist_free_all( m_httpHeaders );
        }
        return false;
    }

    // remove headers
    if( m_httpHeaders ){
        curl_slist_free_all( m_httpHeaders );
    }
    return true;
}

void HTTPClient::fetchCookies(){

    m_cookies.clear();

    struct curl_slist * cookies;
    CURLcode res = curl_easy_getinfo( m_curlSession, CURLINFO_COOKIELIST, & cookies );

    if( CURLE_OK == res ){

        while( cookies->data ){

            string cookieData = cookies->data;

            // get token val
            int tabPos = cookieData.find_last_of("\t") + 1;
            const string tokenVal = cookieData.substr( tabPos, cookieData.size() - tabPos );

            // delete token val from string
            tabPos -= 1;
            cookieData.erase( tabPos, cookieData.size() - tabPos );

            // get token key
            tabPos = cookieData.find_last_of("\t") + 1;
            const string tokenKey = cookieData.substr( tabPos, cookieData.size() - tabPos );

            m_cookies.insert( {tokenKey, tokenVal} );

            if( cookies->next ){
                cookies = cookies->next;
            }
            else{
                cookies->data = nullptr;
            }
        }
    }
}

const std::map<std::string,std::string> & HTTPClient::getCookieTokens(){

    return m_cookies;
}

void HTTPClient::clearCookies(){

    m_cookies.clear();

    curl_easy_setopt( m_curlSession, CURLOPT_COOKIELIST, "ALL" );
}

bool HTTPClient::checkServerAlive( const std::string & _url ){

    // TODO
    m_resultStr.clear();

    char errorBuffer[CURL_ERROR_SIZE] = "";
    long failOnError = 0;
    long httpCode = 0;

    //url-строка, посылаемая на сервер
    curl_easy_setopt(m_curlSession,CURLOPT_URL,_url.c_str());
    curl_easy_setopt(m_curlSession,CURLOPT_ERRORBUFFER, & errorBuffer );
    curl_easy_setopt(m_curlSession,CURLOPT_FAILONERROR, & failOnError );

    CURLcode curlCodeResult = curl_easy_perform(m_curlSession);

    curl_easy_getinfo( m_curlSession, CURLINFO_RESPONSE_CODE, httpCode );

    if( CURLE_OK == curlCodeResult /*&& 200 == httpCode*/ ){

        return true;
    }
    else{
        VS_LOG_ERROR << PRINT_HEADER << " CURL Check Server Status FAIL: " << httpCode << " ( " << _url << " )" << endl;
        VS_LOG_ERROR << PRINT_HEADER << " HTTP-code: " << httpCode << endl;
        VS_LOG_ERROR << PRINT_HEADER << " curl code: " << curl_easy_strerror(curlCodeResult) << endl;
        VS_LOG_ERROR << PRINT_HEADER << " error buffer: " << errorBuffer << endl;
        VS_LOG_ERROR << PRINT_HEADER << " fail on error: " << failOnError << endl;
        return false;
    }
}

string * HTTPClient::receiveAnswer(){
    return & m_resultStr;
}

uint64_t HTTPClient::getResponseSize(){
    return m_resultStr.size();
}

