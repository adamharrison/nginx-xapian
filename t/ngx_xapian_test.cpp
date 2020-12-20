#include <string>
#include <gtest/gtest.h>
#include "../src/ngx_xapian_search.h"

using namespace std;

TEST(sanity, build) {
    ngx_xapian_build_index("t/test_corpus", "en", "/tmp/test_index");
    ASSERT_STREQ(ngx_xapian_get_error(), nullptr);
}

struct ChunkStructure {
    char* pointer;
    int offset;
};

TEST(sanity, search) {
    char buffer[32*1024];
    ChunkStructure chunk_structure = { buffer, 0 };
    int totalLength = ngx_xapian_search_index_json("/tmp/test_index", "en", "Project", 12, +[](const char* chunk, unsigned int chunkSize, void* pointer) {
        ChunkStructure* chunk_structure = (ChunkStructure*)pointer;
        memcpy(&chunk_structure->pointer[chunk_structure->offset], chunk, chunkSize);
        chunk_structure->offset += chunkSize;
    }, &chunk_structure);
    buffer[totalLength] = 0;
    ASSERT_STREQ(ngx_xapian_get_error(), nullptr);

}

TEST(sanity, tmpl) {
    char buffer[32*1024];
    ChunkStructure chunk_structure = { buffer, 0 };
    char tmpl[] = "<ul class='search-results'>{% for result in search.results %}<li><a href='{{ result.url }}'><div class='title'>{{ result.title | escape }}</div><div class='description'>{{ result.description | escape }}</div></a></li>{% endfor %}</ul>";
    void* tmpl_struct = ngx_xapian_parse_template(tmpl, sizeof(tmpl)-1);
    int totalLength = ngx_xapian_search_template("/tmp/test_index", "en", "Project", 12, tmpl_struct, +[](const char* chunk, unsigned int chunkSize, void* pointer) {
        ChunkStructure* chunk_structure = (ChunkStructure*)pointer;
        memcpy(&chunk_structure->pointer[chunk_structure->offset], chunk, chunkSize);
        chunk_structure->offset += chunkSize;
    }, &chunk_structure);
    buffer[chunk_structure.offset] = 0;
    ASSERT_STREQ(buffer, "<ul class='search-results'><li><a href='//test.com/document1'><div class='title'>Document 1</div><div class='description'>Description 1</div></a></li><li><a href='//test.com/document1'><div class='title'>Document 2</div><div class='description'>Description 2</div></a></li></ul>");
    ASSERT_STREQ(ngx_xapian_get_error(), nullptr);

}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};

