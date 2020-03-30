#ifndef COMMAND_POOL_H
#define COMMAND_POOL_H

#include <vector>
#include <memory>

template< typename T_Type, typename... T_Arg >
class ObjectPool
{
public:
    ObjectPool(){}

    std::shared_ptr<T_Type> getInstance( T_Arg... _args ){

        // use existing
        for( const std::shared_ptr<T_Type> & instance : m_objects ){
            if( instance.unique() ){
                instance->clear();
                return instance;
            }
        }

        // or create new
        std::shared_ptr<T_Type> instance = std::make_shared<T_Type>( _args... );
        m_objects.push_back( instance );
        return instance;
    }


private:
    std::vector<std::shared_ptr<T_Type>> m_objects;
};

#endif // COMMAND_POOL_H
