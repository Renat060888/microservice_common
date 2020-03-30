#ifndef ERROR_ENTITY_H
#define ERROR_ENTITY_H

#include <string>

class ErrorEntity
{
public:
    ErrorEntity( const std::string & _errorSource,
                 const std::string & _errorMsg,
                 const std::string _fileName,
                 int _lineNumber );

    const std::string & explain(){ return m_stringRepresentation; }


private:
    std::string m_stringRepresentation;
    std::string m_errorSource;
    int m_lineNumber;
    int m_fileName;


};

#endif // ERROR_ENTITY_H
