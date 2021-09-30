addtest(TransactionDataTest
        TransactionDataTest.cpp
        )
target_link_libraries(TransactionDataTest
        GeniusSDK
        )

addtest(TransactionBlocksTest
        TransactionBlocksTest.cpp
        )
target_link_libraries(TransactionBlocksTest
        GeniusSDK
        )
