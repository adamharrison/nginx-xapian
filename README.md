

## Introduction

nginx-xapian is an nginx module that allows you to index and search a static directory of html files. It's designed to be used with static sites to provide an easy-to-use, high-performance, out-of-the-box search experience without
spinning up anything memory heavy, or complicated to maintain. It's designed to be as minimal of an integration as possible; will almost all aspects of the search controlled by the contents of your documents
rather than by nginx directives.

## Quickstart

Starting from a standard ubuntu install, assuming nginx is already installed, one can do the following (you may have to install some additional libraries depending on your exact nginx config);

    sudo apt-get -y install git mercurial nginx libxapian-dev libpcre2-dev libpcre3-dev libxslt1-dev libgeoip-dev libssl-dev libsass-dev g++ build-essential cmake &&
    git clone https://github.com/adamharrison/nginx-xapian.git && cd nginx-xapian && git submodule update --init &&
    cd liquid-cpp && rm -rf build && mkdir build && cd build && cmake .. && make && sudo make install && cd ../.. && make library &&
    hg clone https://hg.nginx.org/nginx &&
    cd nginx && echo `nginx -V 2>&1 | grep "configure" | sed "s/^configure arguments: /auto\/configure /" | sed -E "s/\-\-add-dynamic-module\S+nginx-xapian\S+//g"` --add-dynamic-module=`pwd`/.. | sh && make &&
    sudo service nginx stop &&
    sudo make install && sudo cp objs/nginx `which nginx` && cd ../../ &&
    sudo service nginx start

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

In addition, if you want to exclude a particular HTML node from being indexed, you can also specify `nointernalindex` as an class of that node.

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

Takes between arguments. The first is the the directory to search. If specified, will build an index for the designated location.

### `xapian_index`

Takes exactly one argument; the path to store the index in. By default, this will be in the nginx folder root folder.

### `xapian_template`

Takes exactly one argument; the path to an HTML/liquid file.

If no template is specified, the only way that the endpoint can be accessed is with an `Accept: application/json` header.

Elements are added exactly as specified below. An example of what a template could look like is:

```liquid
<ul class='search-results'>
	{% for result in search.results %}
		<li>
			<a href='{{ result.url }}'>
				<div class='title'>{{ result.title | escape }}</div>
				<div class='description'>{{ result.description | escape }}</div>
			</a>
		</li>
	{% else %}
		No results found.
	{% endfor %}
</ul>
```

#### search

The object that represents a completed search. If it's nil, no search has been performed.

#### search.terms

The string that was searched.

#### search.results

The array of results. Each result has the following variables:

##### search.results.first.title

The title of a search result.

##### search.results.first.description

The descrpition of the result.

##### search.results.first.url

The URL to the search result.

## Dependencies

* [nginx](https://www.nginx.com/)
* [libxapian](https://xapian.org/)
* [liquid-cpp](https://github.com/adamharrison/liquid-cpp)

## Status

Module is functional, but not polished. Will be expanding as needed to support personal static site. Not efficient at indexing yet; reindexes on every bootup; should probably be a bit
more intelligent about how that's done based on timestamps. Module is almost assuredly not safe, *do not use in production code*.

## TL;DR

Good SEO practices will also net you a better nginx-xapian experience! Just follow those, and you'll be fine. Not ready for production, yet.

## License

### MIT License

Copyright 2020 Adam Harrison

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

