extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
    #include <math.h>
};
#include "ngx_xapian_search.h"

typedef struct {
    ngx_str_t directory;
    ngx_str_t index;
    ngx_str_t tmpl;
} ngx_xapian_search_conf_t;


static char * ngx_xapian_search_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static void * ngx_xapian_search_create_loc_conf(ngx_conf_t *cf);
static ngx_int_t ngx_xapian_search_handler(ngx_http_request_t *r);
static ngx_int_t ngx_xapian_search_init(ngx_conf_t *cf);


static ngx_command_t  ngx_xapian_search_commands[] = {
    {
        ngx_string("xapian_search"),
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
        NGX_CONF_TAKE2|NGX_HTTP_LOC_CONF,
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

static void ngx_xapian_chunk_handler(const char* chunk, unsigned int chunk_size, void* data) {
    memcpy(data, chunk, chunk_size);
}

static ngx_int_t ngx_xapian_search_handler(ngx_http_request_t *r) {
    u_char          *p, *ampersand, *equal, *last;
    ngx_int_t       rc;
    ngx_chain_t     out;
    ngx_buf_t       *buffer;
	ngx_xapian_search_conf_t *config;

	config = (ngx_xapian_search_conf_t*)ngx_http_get_module_loc_conf(r, ngx_xapian_search_module);

        /* only accept gets */
	if (!(r->method & NGX_HTTP_GET))
            return NGX_HTTP_NOT_ALLOWED;
        /* set all headers ahead of time. */
        r->headers_out.content_type.len = sizeof("application/json; charset=UTF-8") - 1;
	r->headers_out.content_type.data = (u_char*)"application/json; charset=UTF-8";
	r->headers_out.status = NGX_HTTP_OK;
	/* parse out the q= query parameter, into the query buffer; 1k of characters should be enough for anybody. */
    unsigned char query[1024];
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
            ngx_cpystrn(query, p, length > sizeof(query) ? sizeof(query) : length);
            break;
        }
    }

    /* 100k should be enough for anyone. (for now) */
    buffer = ngx_create_temp_buf(r->pool, 100*1024);
    if (buffer == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    int total_length = ngx_xapian_search_search_index_json((char*)config->directory.data, "en", (char*)query, 12, ngx_xapian_chunk_handler, buffer->pos);
    if (total_length < 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, ngx_errno, "ngx_xapian_search failed: %s", ngx_xapian_get_error());
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    buffer->last = buffer->pos + total_length;
    buffer->last_buf = 1;
    r->headers_out.content_length_n = total_length;

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

    cmcf = (ngx_http_core_main_conf_t*)ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

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
	conf->directory.len = 0;
	conf->directory.data = NULL;
	conf->index.len = 0;
	conf->index.data = NULL;
	conf->tmpl.len = 0;
	conf->tmpl.data = NULL;
	return conf;
}

static char* ngx_xapian_search_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
	ngx_xapian_search_conf_t *prev = (ngx_xapian_search_conf_t*)parent;
	ngx_xapian_search_conf_t *conf = (ngx_xapian_search_conf_t*)child;
	ngx_conf_merge_str_value(conf->directory, prev->directory, "");
	ngx_conf_merge_str_value(conf->index, prev->index, "");
	return NGX_CONF_OK;
}
