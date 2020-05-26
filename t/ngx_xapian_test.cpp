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
    char tmpl[] = "<ul class='search-results'>{{ results }}</ul>";
    ngx_xapian_template_t* tmpl_struct = ngx_xapian_parse_template(tmpl, sizeof(tmpl)-1);
    int totalLength = ngx_xapian_search_template("/tmp/test_index", "en", "Project", 12, tmpl_struct, +[](const char* chunk, unsigned int chunkSize, void* pointer) {
        ChunkStructure* chunk_structure = (ChunkStructure*)pointer;
        memcpy(&chunk_structure->pointer[chunk_structure->offset], chunk, chunkSize);
        chunk_structure->offset += chunkSize;
    }, &chunk_structure);
    buffer[chunk_structure.offset] = 0;
    ASSERT_STREQ(buffer, "<ul class='search-results'><li><a href='/2020-05/projects-up'><div class='title'>Over the Bridge - Projects Up!</div><div class='description'>I've added in a project sections up above to showcase some of the stuff I'm doing, and its current progress. So far, I've just got my writing stuff in there: some details about my first book, The First Estate (probable title), and a small stub about my ...</div></a></li><li><a href='/projects/the-first-estate'><div class='title'>Over the Bridge - The First Estate</div><div class='description'>The First Estate So, The First Estate is the tentative title for my first book. It's sort of young adult, I suppose, in that it follows a teenage protagonist, but I'm trying to steer in more the direction of blank' href=\\\"https://www.fictionpress.com/s/2...</div></a></li><li><a href='/2020-04/game-programming-synopsis'><div class='title'>Over the Bridge - Game Programming Synopsis</div><div class='description'>So, as of December last year, ish, I started developing a desktop game in C++. This all started as a throwback to development I used to do back in the day. I really like game programming, especially if I get to write the engine. So even though it's obvi...</div></a></li><li><a href='/2020-04/what-to-do-with-free-time'><div class='title'>Over the Bridge - What to do with free time?</div><div class='description'>WARNING: LONG I figure I'll start off with a relatively long post to give the blog a bit of meat. This post is mainly for me (I've never actually written out the whole story of this before), so it'll meander a lot before it comes back to the title quest...</div></a></li></ul>");
    ASSERT_STREQ(ngx_xapian_get_error(), nullptr);

}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};

