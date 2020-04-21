#ifndef CLASS_FACTORY_H
#define CLASS_FACTORY_H

#include <cassert>
#include <string>
#include <map>
#include <memory>
#include <functional>

// TODO: remove this dependence
namespace rti_client_vega {
    class RTIObject;
}

// creator
template< typename T >
std::shared_ptr<rti_client_vega::RTIObject> createClass( const std::string & _instanceName ){
    return std::make_shared<T>( _instanceName );
}

template< typename T >
class Creator {
public:
    std::shared_ptr<rti_client_vega::RTIObject> operator()(){
        return std::make_shared<T>();
    }
};


using FPCreateInstanceOfClass = std::shared_ptr<rti_client_vega::RTIObject>( * )( const std::string & );

// factory
class ClassFactory {
public:
    static ClassFactory & singleton(){
        static ClassFactory instance;
        return instance;
    }

    template< typename T >
    void addClassFactory( const std::string _className ){

        auto iter = m_classes.find( _className );
        if( iter == m_classes.end() ){

            // function ptr ( TODO: try via lambda, functor, std::function, ... )
            m_classes.insert( {_className, createClass<T> } );
        }
    }

    std::shared_ptr<rti_client_vega::RTIObject> createInstanceOf( const std::string & _className, const std::string & _instanceName ){
        auto iter = m_classes.find( _className );
        assert( iter != m_classes.end() );
        return iter->second(_instanceName);
    }


private:
    ClassFactory(){}
    ~ClassFactory(){}

    ClassFactory( const ClassFactory & _rhs ) = delete;
    ClassFactory & operator=( const ClassFactory & _rhs ) = delete;

    std::map<std::string, FPCreateInstanceOfClass> m_classes;
};
#define CLASS_FACTORY ClassFactory::singleton()

#endif // CLASS_FACTORY_H
