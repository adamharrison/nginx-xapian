#ifndef SEARCH_H
#define SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef void (xapian_chunk_callback)(const char*, unsigned int, void*);
    typedef xapian_chunk_callback* xapian_chunk_callbackp;

    const char* ngx_xapian_get_error();
    void ngx_xapian_clear_error();

    int ngx_xapian_build_search_index(const char* directory, const char* language, const char* target);
    int ngx_xapian_search_search_index(const char* index, const char* language, const char* query, int max_results, xapian_chunk_callbackp chunkCallback, void* data);
    int ngx_xapian_search_search_index_json(const char* index, const char* language, const char* query, int max_results, xapian_chunk_callbackp chunkCallback, void* data);
#ifdef __cplusplus
};
#endif

#endif
