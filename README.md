

## Introduction

nginx-xapian is an nginx module that allows you to index and search a static directory of html files. It's designed to be used with static sites to provide an easy-to-use out-of-the-box search experience without
spinning up anything memory heavy, or complicated to maintain. It's designed to be as minimal of an integration as possible; will almost all aspects of the search controlled by the contents of your documents
rather than by nginx directives.

## Usage

Using nginx-xapian is really simple. In your nginx config add the following line:

	load_module <path_to_xapian_shared_object>;

To add in an endpoint that will search your files, you simply add the following location block, wherever you'd like search capability

	location / {
		xapian_search <path_to_directory>;
	}
	
And that's it. This will return a JSON list of hits against files in that directory, based on the query parameter `q`. It'll treat the files as a normal search engine would treat them; returning the `<title>`, as well as `<meta name="description">`. Will index any HTML file in the directory specified, though will specifically omit any file that has a meta attribute tag of the following:
	
nginx-xapian will pay attention to the following head tags to influence how and what it searches:

### <title>

Heavily weighted score. Is returned in the main result.

### <meta name="keywords">

Heavily weighted score. Not returned in the main result.

### <meta name="description">

Middle weight score. Not returned in the main result.

### `<meta name="robots">`

Used to determine whether or not a file should be indexed. Supply `nointernalindex` like so:

	<meta name="robots" content="nointernalindex">

Will result in the file not being indexed, even if it's in the correct folder.

### `<meta name="langauge">`

As with search engines, specifying the language will affect how stemming works and such. You can also filter which language you're searching for by specfying `language` as a query parameter to the xapian endpoint.


## TL;DR

Good SEO practices will also net you a better nginx-xapian experience! Just follow those, and you'll be fine.

## License

### MIT License

Copyright 2020 Adam Harrison

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

