#ifndef NGX_XAPIAN_SEARCH_H
#define NGX_XAPIAN_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif
    struct ngx_xapian_result_s {
        const char* pointer;
        size_t length;
    };
    typedef struct ngx_xapian_result_s ngx_xapian_result_t;

    typedef void (ngx_xapian_chunk_callback)(const char*, unsigned int, void*);
    typedef ngx_xapian_chunk_callback* ngx_xapian_chunk_callbackp;

    typedef void (ngx_xapian_result_callback)(ngx_xapian_result_s, void*);
    typedef ngx_xapian_result_callback* ngx_xapian_result_callbackp;

    const char* ngx_xapian_result_get_title(ngx_xapian_result_t* result, size_t* len);
    const char* ngx_xapian_result_get_description(ngx_xapian_result_t* result, size_t* len);
    const char* ngx_xapian_result_get_path(ngx_xapian_result_t* result, size_t* len);
    const char* ngx_xapian_result_get_url(ngx_xapian_result_t* result, size_t* len);


    const char* ngx_xapian_get_error();
    void ngx_xapian_clear_error();

    int ngx_xapian_build_search_index(const char* directory, const char* language, const char* target);
    int ngx_xapian_search_search_index(const char* index, const char* language, const char* query, int max_results, ngx_xapian_result_callbackp resultCallback, void* data);
    int ngx_xapian_search_search_index_json(const char* index, const char* language, const char* query, int max_results, ngx_xapian_chunk_callbackp chunkCallback, void* data);
#ifdef __cplusplus
};
#endif

#endif
