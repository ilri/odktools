json2csv-cpp
============

###JSON to csv converter in C++.
__Features__
+ Extremely fast
+ Recursively flattens shallow or deeply nested JSON

__Motivation__

Regardless of how I prettify JSON, my target audience find table and spreadsheet representation of data more intuitive.
This author of this [article](http://sunlightfoundation.com/blog/2014/03/11/making-json-as-simple-as-a-spreadsheet/) articulates this fact well.

__Usage__
```bash
$ echo basic
$ json2csv "test.json" "test.csv"
$ echo "download json from web and transform"
$ wget http://www.treasury.gov/jsonfiles/data.json > data.json & json2csv data.json data.csv
```

__Dependencies__
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp) Open source JSON reader/writer