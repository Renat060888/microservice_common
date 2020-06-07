#ifndef DATABASE_MANAGER_BASE_TEST_H
#define DATABASE_MANAGER_BASE_TEST_H

#include <gtest/gtest.h>

#include "storage/database_manager_base.h"

class TestDatabaseManagerBase : public ::testing::Test
{
public:
    TestDatabaseManagerBase();


protected:
    static void SetUpTestCase();
    static void TearDownTestCase();

//    void metadata_test_recorder();
//    void payload_test_recorder();
//    void description_test_recorder();

    static DatabaseManagerBase * m_database;
};

#endif // DATABASE_MANAGER_BASE_TEST_H
