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
    char tmpl[] = "<ul class='search-results'>{% for result in search.results %}<li><a href='{{ result.url }}'><div class='title'>{{ result.title | escape }}</div><div class='description'>{{ result.description | escape }}</div></a>{% endfor %}</ul>";
    void* tmpl_struct = ngx_xapian_parse_template(tmpl, sizeof(tmpl)-1);
    int totalLength = ngx_xapian_search_template("/tmp/test_index", "en", "Project", 12, tmpl_struct, +[](const char* chunk, unsigned int chunkSize, void* pointer) {
        ChunkStructure* chunk_structure = (ChunkStructure*)pointer;
        memcpy(&chunk_structure->pointer[chunk_structure->offset], chunk, chunkSize);
        chunk_structure->offset += chunkSize;
    }, &chunk_structure);
    buffer[chunk_structure.offset] = 0;
    ASSERT_STREQ(buffer, "<ul class='search-results'><li><a href='//overthebridge.ca/2020-05/projects-up'><div class='title'>Projects Up!</div><div class='description'>I&apos;ve added in a project sections up above to showcase some of the stuff I&apos;m doing, and its current progress. So far, I&apos;ve just got my writing stuff in there: some details about my first book, The First Estate (probable title), and a small stub about my ...</div></a><li><a href='//overthebridge.ca/projects/the-first-estate'><div class='title'>The First Estate</div><div class='description'>So, The First Estate is the tentative title for my first book. It&apos;s sort of young adult, I suppose, in that it follows a teenage protagonist, but I&apos;m trying to steer in more the direction of Mother of Learning, rather than Harry Potter. It&apos;s going to be...</div></a><li><a href='//overthebridge.ca/2020-04/game-programming-synopsis'><div class='title'>Game Programming Synopsis</div><div class='description'>So, as of December last year, ish, I started developing a desktop game in C++. This all started as a throwback to development I used to do back in the day. I really like game programming, especially if I get to write the engine. So even though it&apos;s obvi...</div></a><li><a href='//overthebridge.ca/2020-04/what-to-do-with-free-time'><div class='title'>What to do with free time?</div><div class='description'>WARNING: LONG I figure I&apos;ll start off with a relatively long post to give the blog a bit of meat. This post is mainly for me (I&apos;ve never actually written out the whole story of this before), so it&apos;ll meander a lot before it comes back to the title quest...</div></a></ul>");
    ASSERT_STREQ(ngx_xapian_get_error(), nullptr);

}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};

