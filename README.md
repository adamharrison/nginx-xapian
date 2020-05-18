

## Introduction

nginx-xapian is an nginx module that allows you to index and search a static directory of html files. It's designed to be used with static sites to provide an easy-to-use, high-performance, out-of-the-box search experience without
spinning up anything memory heavy, or complicated to maintain. It's designed to be as minimal of an integration as possible; will almost all aspects of the search controlled by the contents of your documents
rather than by nginx directives.

## Quickstart

Using nginx-xapian is really simple. In your nginx config add the following line:

	load_module <path_to_xapian_shared_object>;

To add in an endpoint that will search your files, you simply add the following location block, wherever you'd like search capability

	location /search {
		xapian_search <path_to_directory>;
	}

And that's it. This will return a JSON list of hits against files in that directory, based on the query parameter `q`. It'll treat the files as a normal search engine would treat them; returning the `<title>`, as well as `<meta name="description">`. Will index any HTML file in the directory specified, though will specifically omit any file that has a meta attribute tag of the following:

nginx-xapian will pay attention to the following head tags to influence how and what it searches:

## Documents

By default, every HTML file is indexed. This can be modified through the use of `<meta name="robots">`. See below.

### `<title>`

Heavily weighted score. Is returned in the main result.

### `<meta name="keywords">`

Heavily weighted score. Not returned in the main result.

### `<meta name="description">`

Middle weight score. Not returned in the main result.

### `<meta name="robots">`

Used to determine whether or not a file should be indexed. Supply `nointernalindex` like so:

	<meta name="robots" content="nointernalindex">

Will result in the file not being indexed, even if it's in the correct folder.

### `<meta name="langauge">`

As with search engines, specifying the language will affect how stemming works and such. You can also filter which language you're searching for by specfying `language` as a query parameter to the xapian endpoint.

### `<link rel="canonical">`

Will return this URL with search results, otherwise will omit urls entirely.

## Query Paramters

### q

This contains the actual query searched.

### results

Contains the requested amount of results. Allows up to 100. Default 12.

### page

Contains the requested page.

## Nginx Directives

The following nginx directives are supported at the `location` block level.

### xapian_search

Takes exactly one argument; the directory to search. If specified, searching is enabled at this location.

### xapian_index

Takes exactly one argument; the path to store the index in. By default, this will be in the nginx folder root folder.

### xapian_template

Takes exactly two arguments. The first, is a path to an HTML file. The second is a CSS selector that corresponds to the container to add the search result HTML elements into. Elements are added exactly like so.

    <ul class='search-results'>
        <li><a href='/'>
            <div class='title'>{{ result[0].title }}</div>
            <div class='description'>{{ result[0].description }}</div>
        </a></li>
    </ul>

If no template is specified, the only way that the endpoint can be accessed is with an `Accept: application/json` header.


## Dependencies

Currently nginx-xapian has no depdenncies beyond nginx, and xapian. This may change depending on whether my hack-y JSON output, and faux-XML parsing are adeauate to the task. If not, this project will also further
include tinyxml, as well as rapidjson.

## Status

Initial commit. Not functional yet. Soon, hopefully. Goal is to get everything functioning; then will expand out from there.

## TL;DR

Good SEO practices will also net you a better nginx-xapian experience! Just follow those, and you'll be fine. Not ready for production (or even debug, yet). Could potentially include one of the well-supported C++ templating
enignes to provide an actual HTML page rather than JSON, but am unsure if I want to eventually do that. May simply take a CSS selector, a reference page, and render a <ul> into the page. Will see.

## License

### MIT License

Copyright 2020 Adam Harrison

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

