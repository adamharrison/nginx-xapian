

## Introduction

nginx-xapian is an nginx module that allows you to index and search a static directory of html files. It's designed to be used with static sites to provide an easy-to-use, high-performance, out-of-the-box search experience without
spinning up anything memory heavy, or complicated to maintain. It's designed to be as minimal of an integration as possible; will almost all aspects of the search controlled by the contents of your documents
rather than by nginx directives.

## Quickstart

Starting from a standard ubuntu install, assuming nginx is already installed, one can do the following (you may have to install some additional libraries depending on your exact nginx config);

    sudo apt-get -y git install mercurial nginx libxapian-dev libpcre2-dev libpcre3-dev libxslt1-dev libgeoip-dev libssl-dev
    git clone https://github.com/adamharrison/nginx-xapian.git
    cd nginx-xapian && make library && cd ..
    hg clone https://hg.nginx.org/nginx
    cd nginx
    auto/configure `nginx -V` --add-dynamic-module=`pwd`/../nginx-xapian && make && sudo make install && sudo cp objs/nginx `which nginx`
    sudo service nginx restart

This will compile nginx with the same settings as your existing nginx install, while also adding in nginx-xapian. **Note that this will overwrite your current nginx install.**

Using nginx-xapian is really simple. In your nginx config (usualy `/etc/nginx/nginx.conf`) add the following line outside all the blocks, near the top:

	load_module modules/ngx_xapian_search_module.so;

To add in an endpoint that will search your files, you simply add the following location block, wherever you'd like search capability

	location /search {
		xapian_search on;
	}

And that's it. This will return a JSON list of hits against files in the root directory, based on the query parameter `q`. It'll treat the files as a normal search engine would treat them; returning the `<title>`, as well as `<meta name="description">`. Will index any HTML file in the directory specified, though will specifically omit any file that has a meta attribute tag of the following:

nginx-xapian will pay attention to the following head tags to influence how and what it searches:

## Documents

By default, every HTML file is indexed. This can be modified through the use of `<meta name="robots">`. See below.

### `<title>`

Heavily weighted score. Is returned in the main result.

### `<meta name="description">`

Middle weight score. Returned in the main result.

### `<meta name="keywords">`

Heavily weighted score. Not returned in the main result.

### `<meta name="robots">`

Used to determine whether or not a file should be indexed. Supply `nointernalindex` like so:

	<meta name="robots" content="nointernalindex">

Will result in the file not being indexed, even if it's in the correct folder.

### `<meta name="langauge">`

As with search engines, specifying the language will affect how stemming works and such. You can also filter which language you're searching for by specfying `language` as a query parameter to the xapian endpoint.

### `<link rel="canonical">`

Will return this URL with search results, otherwise will omit urls entirely.

## Query Paramters

### `q`

This contains the actual query searched.

### `results`

Contains the requested amount of results. Allows up to 100. Default 12.

### `page`

Contains the requested page.

## Nginx Directives

The following nginx directives are supported at the `location` block level.

### `xapian_search`

Takes a single argument, `on`. Allows this endpoint to return saerch results.

### `xapian_directory`

Takes at most argument; the directory to search. If specified, will build an index for the designated lcoation. If unspecified, will search the `root` folder.

### `xapian_index`

Takes exactly one argument; the path to store the index in. By default, this will be in the nginx folder root folder.

### `xapian_template`

Takes exactly one argument; the path to an HTML file. The following tags are supported as part of a super-simple templating engine. In future, this may
link against a true template library, but for now, in order to not introduce any dependencies, note that these tags are *simple literals*.

If no template is specified, the only way that the endpoint can be accessed is with an `Accept: application/json` header.

Elements are added exactly as specified below.

#### {{ results }}

The search results.

    <ul class='search-results'>
        <li><a href='{{ reuslt[0].url | escape }}'>
            <div class='title'>{{ result[0].title | escape_html }}</div>
            <div class='description'>{{ result[0].description | escape_html }}</div>
        </a></li>
    </ul>

#### {{ search }}

The search terms used.

#### {{ search_escaped }}

The search terms used, but escaped. Suitable for sticking into an input box value.

## Dependencies

Currently nginx-xapian has no depdenncies beyond nginx, and xapian. This may change depending on whether my hack-y JSON output, and faux-XML parsing are adeauate to the task. If not, this project will also further
include tinyxml, as well as rapidjson.

## Status

Module is functional, but not polished. Will be expanding as needed to support personal static site. Not efficient at indexing yet; reindexes on every bootup; should probably be a bit
more intelligent about how that's done based on timestamps.

## TL;DR

Good SEO practices will also net you a better nginx-xapian experience! Just follow those, and you'll be fine. Not ready for production (or even debug, yet). Could potentially include one of the well-supported C++ templating
enignes to provide an actual HTML page rather than JSON, but am unsure if I want to eventually do that. May simply take a CSS selector, a reference page, and render a `<ul>` into the page. Will see.

## License

### MIT License

Copyright 2020 Adam Harrison

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

