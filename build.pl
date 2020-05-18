#!/usr/bin/env perl

use strict;
use warnings;

use Cwd 'abs_path';
use File::Basename;

# Go to the nginx file, configure, and build.

system("cd " . $ENV{"HOME"} . "/nginx/nginx; auto/configure --add-dynamic-module=" . abs_path(dirname(__FILE__)));
system("cd " . $ENV{"HOME"} . "/nginx/nginx; make -j 8");