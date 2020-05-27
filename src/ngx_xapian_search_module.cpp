extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
    #include <math.h>
    #include <sys/stat.h>
};
#include "ngx_xapian_search.h"

typedef struct {
    ngx_flag_t enabled;
    ngx_str_t directory;
    ngx_str_t index;
    ngx_str_t tmpl;
    ngx_xapian_template_t* tmpl_contents;
} ngx_xapian_search_conf_t;


static char * ngx_xapian_search_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static void * ngx_xapian_search_create_loc_conf(ngx_conf_t *cf);
static ngx_int_t ngx_xapian_search_handler(ngx_http_request_t *r);
static ngx_int_t ngx_xapian_search_init(ngx_conf_t *cf);


static ngx_command_t  ngx_xapian_search_commands[] = {
    {
        ngx_string("xapian_search"),
        NGX_CONF_TAKE1|NGX_HTTP_LOC_CONF,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_xapian_search_conf_t, enabled),
        NULL
    }, {
        ngx_string("xapian_directory"),
        NGX_CONF_TAKE1|NGX_HTTP_LOC_CONF,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_xapian_search_conf_t, directory),
        NULL
    }, {
        ngx_string("xapian_index"),
        NGX_CONF_TAKE1|NGX_HTTP_LOC_CONF,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_xapian_search_conf_t, index),
        NULL
    }, {
        ngx_string("xapian_template"),
        NGX_CONF_TAKE1|NGX_HTTP_LOC_CONF,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_xapian_search_conf_t, tmpl),
        NULL
    },
    ngx_null_command
};


static ngx_http_module_t ngx_xapian_search_module_ctx = {
    NULL, /* preconfiguration */
    ngx_xapian_search_init, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    ngx_xapian_search_create_loc_conf, /* create location configration */
    ngx_xapian_search_merge_loc_conf /* merge location configration */
};


ngx_module_t  ngx_xapian_search_module = {
    NGX_MODULE_V1,
    &ngx_xapian_search_module_ctx,                   /* module context */
    ngx_xapian_search_commands,                      /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

struct ngx_xapian_chunk_handler_data_t {
    char* dst;
    unsigned int offset;
};

static void ngx_xapian_chunk_handler(const char* chunk, unsigned int chunk_size, void* data) {
    ngx_xapian_chunk_handler_data_t* handler_data = (ngx_xapian_chunk_handler_data_t*)data;
    memcpy(&handler_data->dst[handler_data->offset], chunk, chunk_size);
    handler_data->offset += chunk_size;
}

static ngx_int_t ngx_xapian_search_handler(ngx_http_request_t *r) {
    u_char          *p, *ampersand, *equal, *last;
    ngx_int_t       rc;
    ngx_chain_t     out;
    ngx_buf_t       *buffer;
	ngx_xapian_search_conf_t *config;

	config = (ngx_xapian_search_conf_t*)ngx_http_get_module_loc_conf(r, ngx_xapian_search_module);

	if (config->enabled != 1)
        return NGX_DECLINED;

    /* only accept gets */
	if (!(r->method & NGX_HTTP_GET))
        return NGX_HTTP_NOT_ALLOWED;

    unsigned char query[1024] = "";
    p = r->args.data;
    last = p + r->args.len;
    for ( ; p < last; p++) {
        ampersand = ngx_strlchr(p, last, '&');
        if (ampersand == NULL)
            ampersand = last;
        equal = ngx_strlchr(p, last, '=');
        if ((equal == NULL) || (equal > ampersand))
            equal = ampersand;
        if (equal - p == 1 && p[0] == 'q') {
            unsigned int length = ampersand - equal;
            length = length > sizeof(query) ? sizeof(query) : length;
            ngx_cpystrn(query, equal+1, length > sizeof(query) ? sizeof(query) : length);
            query[length] = 0;
            break;
        }
    }

    char path[PATH_MAX];
    memcpy(path, config->index.data, config->index.len);
    path[config->index.len] = 0;

    const char* index_path = path;
    r->headers_out.status = NGX_HTTP_OK;
    if (r->headers_in.accept && ngx_strstr(r->headers_in.accept->value.data, "json")) {
        /* set all headers ahead of time. */
        r->headers_out.content_type.len = sizeof("application/json; charset=UTF-8") - 1;
        r->headers_out.content_type.data = (u_char*)"application/json; charset=UTF-8";
        /* parse out the q= query parameter, into the query buffer; 1k of characters should be enough for anybody. */
        /* 100k should be enough for anyone. (for now) */
        buffer = ngx_create_temp_buf(r->pool, 100*1024);
        if (buffer == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ngx_xapian_chunk_handler_data_t handler_data = { (char*)buffer->pos, 0 };
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "searching for term %s in index %s, as json", query, index_path);
        int total_length = ngx_xapian_search_index_json(index_path, "en", (char*)query, 12, ngx_xapian_chunk_handler, &handler_data);
        if (total_length < 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, ngx_errno, "ngx_xapian_search failed: %s", ngx_xapian_get_error());
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        buffer->pos[total_length] = 0;

        buffer->last = buffer->pos + total_length;
        buffer->last_buf = 1;
        r->headers_out.content_length_n = total_length;
    } else {
        if (!config->tmpl_contents)
            return NGX_HTTP_NOT_ALLOWED;
        r->headers_out.content_type.len = sizeof("text/html; charset=UTF-8") - 1;
        r->headers_out.content_type.data = (u_char*)"text/html; charset=UTF-8";
        buffer = ngx_create_temp_buf(r->pool, 100*1024);
        if (buffer == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ngx_xapian_chunk_handler_data_t handler_data = { (char*)buffer->pos, 0 };
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "searching for term %s in index as html", query);
        int total_results = ngx_xapian_search_template(index_path, "en", (char*)query, 12, config->tmpl_contents, ngx_xapian_chunk_handler, &handler_data);
        if (total_results < 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, ngx_errno, "ngx_xapian_search failed: %s", ngx_xapian_get_error());
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        buffer->pos[handler_data.offset] = 0;

        buffer->last = buffer->pos + handler_data.offset;
        buffer->last_buf = 1;
        r->headers_out.content_length_n = handler_data.offset;
    }

    /* Send off headers. */
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        ngx_http_finalize_request(r, rc);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Send off buffer. */
    out.buf = buffer;
    out.next = NULL;
	return ngx_http_output_filter(r, &out);
}


static ngx_int_t ngx_xapian_search_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt       *h;
    ngx_http_core_main_conf_t *cmcf;
    // ngx_xapian_search_conf_t* conf;

    cmcf = (ngx_http_core_main_conf_t*)ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	// conf = (ngx_xapian_search_conf_t*)ngx_http_conf_get_module_loc_conf(cf, ngx_xapian_search_module);

    h = (ngx_http_handler_pt*)ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL)
        return NGX_ERROR;

    *h = ngx_xapian_search_handler;

    return NGX_OK;
}

static void* ngx_xapian_search_create_loc_conf(ngx_conf_t *cf) {
	ngx_xapian_search_conf_t *conf;
	conf = (ngx_xapian_search_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_xapian_search_conf_t));
	if (conf == NULL)
		return NGX_CONF_ERROR;
    conf->enabled = NGX_CONF_UNSET;
    conf->tmpl_contents = NULL;
	conf->directory.len = 0;
	conf->directory.data = NULL;
	conf->index.len = 0;
	conf->index.data = NULL;
	conf->tmpl.len = 0;
	conf->tmpl.data = NULL;
	return conf;
}

static char* ngx_xapian_search_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_core_loc_conf_t  *clcf;
	ngx_xapian_search_conf_t *prev = (ngx_xapian_search_conf_t*)parent;
	ngx_xapian_search_conf_t *conf = (ngx_xapian_search_conf_t*)child;

	clcf = (ngx_http_core_loc_conf_t*)ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

	if (prev->enabled != NGX_CONF_UNSET)
        conf->enabled = 1;

	if (conf->enabled == 1) {
        if (conf->directory.data == NULL) {
            if (prev->directory.data == NULL) {
                if (clcf && clcf->root.data) {
                    int length = clcf->root.len;
                    conf->index.data = (unsigned char*)ngx_palloc(cf->pool, length+1);
                    memcpy(conf->directory.data, clcf->root.data, length);
                    conf->directory.data[length] = 0;
                    conf->directory.len = length;
                }
            } else {
                conf->directory = prev->directory;
            }
        }
        char index_buffer[PATH_MAX] = "";
        memcpy(index_buffer, (const char*)conf->directory.data, conf->directory.len+1);
        index_buffer[conf->directory.len+2] = 0;
        strcat(index_buffer, "/xapian_index");
        if (conf->index.data == NULL)  {
            if (prev->index.data != NULL) {
                conf->index = prev->index;
            } else {
                int length = strlen(index_buffer);
                conf->index.data = (unsigned char*)ngx_palloc(cf->pool, length+1);
                memcpy(conf->index.data, index_buffer, length);
                conf->index.data[length] = 0;
                conf->index.len = length;
            }
        }
        ngx_conf_merge_str_value(conf->tmpl, prev->tmpl, "");

        if (!conf->directory.data || conf->directory.len == 0) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Requires a xapian_search directory to be specified, or a root directive.");
            return (char*)NGX_CONF_ERROR;
        }
        if (!conf->index.data || conf->index.len == 0) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Requires a xapian_index directory to be specified.");
            return (char*)NGX_CONF_ERROR;
        }
        if (conf->tmpl.data && conf->tmpl.len > 0) {
            char template_buffer[PATH_MAX] = "";
            if (conf->tmpl.data[0] != '/' && clcf && clcf->root.data) {
                strcat(template_buffer, (char*)clcf->root.data);
                strcat(template_buffer, "/");
            }
            strcat(template_buffer, (char*)conf->tmpl.data);


            int length = strlen(template_buffer);
            conf->tmpl.data = (unsigned char*)ngx_palloc(cf->pool, length+1);
            memcpy(conf->index.data, index_buffer, length);
            conf->tmpl.data[length] = 0;
            conf->tmpl.len = length;

            // Should most assuredly pre-parse this with a real template library, but for now, just replacing at runtime will have to do.
            FILE* file = fopen(template_buffer, "rb");
            if (!file) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Can't find xapian template: %s", template_buffer);
                return (char*)NGX_CONF_ERROR;
            }
            fseek(file, 0, SEEK_END);
            size_t buffer_length = ftell(file);
            fseek(file, 0, SEEK_SET);
            char* buffer = (char*)ngx_palloc(cf->pool, buffer_length+1);
            if (fread(buffer, 1, buffer_length, file) != buffer_length) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Error reading xapian template: %s", template_buffer);
                return (char*)NGX_CONF_ERROR;
            }
            conf->tmpl_contents = ngx_xapian_parse_template(buffer, buffer_length);
            ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "Succesfully parsed xapian search template %s.", template_buffer);
        }


        ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "Building a xapian search index for %s at %s.", conf->directory.data, index_buffer);
        if (ngx_xapian_build_index((const char*)conf->directory.data, "en", (const char*)index_buffer) == 0)
            ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "Succesfully built xapian search index for %s at %s.", conf->directory.data, index_buffer);
        else
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to build xapian search index for %s at %s: %s.", conf->directory.data, index_buffer, ngx_xapian_get_error());
    }
	return NGX_CONF_OK;
}
