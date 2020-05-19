#include <string>
#include <gtest/gtest.h>
#include "../src/ngx_xapian_search.h"

using namespace std;

TEST(sanity, build) {
    ngx_xapian_build_search_index("t/test_corpus", "en", "/tmp/test_index");
    ASSERT_STREQ(ngx_xapian_get_error(), nullptr);
}

struct ChunkStructure {
    char* pointer;
    int offset;
};

TEST(sanity, search) {
    char buffer[32*1024];
    ChunkStructure chunk_structure = { buffer, 0 };
    int totalLength = ngx_xapian_search_search_index_json("/tmp/test_index", "en", "Project", 12, +[](const char* chunk, unsigned int chunkSize, void* pointer) {
        ChunkStructure* chunk_structure = (ChunkStructure*)pointer;
        memcpy(&chunk_structure->pointer[chunk_structure->offset], chunk, chunkSize);
        chunk_structure->offset += chunkSize;
    }, &chunk_structure);
    buffer[totalLength] = 0;
    ASSERT_STREQ(ngx_xapian_get_error(), nullptr);

}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};

