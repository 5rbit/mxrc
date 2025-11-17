#include "gtest/gtest.h"
#include <tbb/concurrent_hash_map.h>
#include <string>

namespace mxrc::core::datastore {
    
TEST(TBBIntegrationTest, ConcurrentHashMapCompilesAndLinks) {
    tbb::concurrent_hash_map<std::string, int> my_map;
    my_map.insert({"test_key", 123});
    ASSERT_TRUE(my_map.count("test_key"));

    // To find and access an element
    tbb::concurrent_hash_map<std::string, int>::accessor a;
    if (my_map.find(a, "test_key")) {
        ASSERT_EQ(a->second, 123);
        a->second = 456; // Modify the value
    } else {
        FAIL() << "Key 'test_key' not found after insertion.";
    }

    // Verify the modified value
    tbb::concurrent_hash_map<std::string, int>::const_accessor ca;
    if (my_map.find(ca, "test_key")) {
        ASSERT_EQ(ca->second, 456);
    } else {
        FAIL() << "Key 'test_key' not found after modification.";
    }
}

} // namespace mxrc::core::datastore