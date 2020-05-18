#include <xapian.h>
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <regex>
#include <algorithm>
#include <exception>

#include "ngx_xapian_search.h"

using namespace std;
using namespace Xapian;


string extract_meta_attribute(const string& document, const string& attribute) {
    string regexBuffer = string("<meta\\s+.*name=[\"']") + attribute + "\"\\s+value\\s*=\\s*";
    static auto metaRegex = regex(regexBuffer.data());
    smatch match;
    if (regex_search(document, match, metaRegex)) {
        char delimiter = document[match.position() + match.length(0)];
        if (delimiter == '"' || delimiter == '\'') {
            for (unsigned int i = match.position(); i < document.size(); ++i) {
                    if (document[i] == delimiter && (i == 0 || document[i-1] != '\\')) {
                        return string(&document[match.position()], min((int)(i - match.position()), 1024));
                    }
            }
        }
    }
    return string();
}


string extract_link_attribute(const string& document, const string& attribute) {
    string regexBuffer = string("<link\\s+.*rel=[\"']") + attribute + "\"\\s+href\\s*=\\s*";
    static auto metaRegex = regex(regexBuffer.data());
    smatch match;
    if (regex_search(document, match, metaRegex)) {
        char delimiter = document[match.position() + match.length(0)];
        if (delimiter == '"' || delimiter == '\'') {
            for (unsigned int i = match.position(); i < document.size(); ++i) {
                    if (document[i] == delimiter && (i == 0 || document[i-1] != '\\')) {
                        return string(&document[match.position()], min((int)(i - match.position()), 1024));
                    }
            }
        }
    }
    return string();
}

bool xapian_index_file(WritableDatabase& database, TermGenerator& termGenerator, const string& path) {
    Document document;
    termGenerator.set_document(document);

    FILE* file = fopen(path.data(), "rb");
    if (!file)
        throw "Can't open file.";
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    string buffer;
    buffer.resize(size);
    if (fread(const_cast<char*>(buffer.data()), sizeof(char), size, file) != size) {
        throw "Can't read whole file.";
    }

    // Rather than using libXML2, just pump these into a regex, and print out the JSON. If it gets more complicated, start using libraries, but for now, this should do.
    string title;
    string description = extract_meta_attribute(buffer, "description");
    string keywords = extract_meta_attribute(buffer, "keywords");
    string robots = extract_meta_attribute(buffer, "robot");
    string language = extract_meta_attribute(buffer, "language");
    string url = extract_link_attribute(buffer, "canonical");
    static auto titleRegex = regex("<\\s*title\\s*>");
    smatch match;
    if (regex_search(buffer, match, titleRegex)) {
        for (unsigned int i = match.position(); i < buffer.size(); ++i) {
            if (strncmp(&buffer[i], "</title>", sizeof("</title>")-1) == 0) {
                title = string(&buffer[match.position()], min((int)(i - match.position()), 1024));
                break;
            }
        }
    }

    if (title.empty() || description.empty() || robots.find("nointernalindex") != string::npos)
        return false;

    // Rather than including rapidJSON, just pump these strings in. If it proves problematic, we'll include the library.
    char outputBuffer[1024*4] = "{";
    unsigned int offset = 1;

    auto copyToJson = [](char* dst, const char* str, int len) {
        char* target = dst;
        for (int i = 0; i < len; ++i) {
            if (str[i] == '"' && (i == 0 || str[i-1] != '\\'))
                *(target++) = '\\';
            *(target++) = str[i];
        }
        return target - dst;
    };
    auto copyToJsonField = [&](char* dst, const char* name, const string& str) {
        char* target = dst;
        int name_length = strlen(name);
        *(target++) = '"';
        target = (char*)memcpy(target, name, name_length + name_length);
        *(target++) = '"';
        *(target++) = ':';
        *(target++) = '"';
        target += copyToJson(target, str.data(), str.size());
        *(target++) = '"';
        return target - dst;
    };

    offset += copyToJsonField(&outputBuffer[offset], "path", path);
    outputBuffer[offset++] = ',';
    offset += copyToJsonField(&outputBuffer[offset], "title", title);
    outputBuffer[offset++] = ',';
    offset += copyToJsonField(&outputBuffer[offset], "description", description);
    if (!url.empty()) {
        outputBuffer[offset++] = ',';
        offset += copyToJsonField(&outputBuffer[offset], "url", url);
    }
    outputBuffer[offset++] = '}';
    outputBuffer[offset] = 0;

    termGenerator.index_text(title.data(), 10);
    termGenerator.increase_termpos();
    termGenerator.index_text(keywords.data(), 3);
    termGenerator.increase_termpos();
    termGenerator.index_text(description.data(), 3);
    termGenerator.increase_termpos();
    termGenerator.index_text(buffer.data());
    termGenerator.increase_termpos();

    document.set_data(outputBuffer);
    document.add_boolean_term(path);
    database.replace_document(path, document);
    return true;
}

void xapian_traverse_index_directories(WritableDatabase& database, TermGenerator& termGenerator, const char* directory) {
    DIR *dir = opendir(directory);
    if (!dir)
        return;
    dirent *dp;
    while ((dp = readdir (dir)) != NULL) {
        char path[PATH_MAX];
        switch (dp->d_type) {
            case DT_DIR: {
                if (strcmp(directory, ".") != 0 && strcmp(directory, "..") != 0) {
                    strcpy(path, directory);
                    strcat(path, dp->d_name);
                    xapian_traverse_index_directories(database, termGenerator, path);
                }
            } break;
            case DT_REG:
                strcpy(path, directory);
                strcat(path, dp->d_name);
                xapian_index_file(database, termGenerator, path);
            break;
        }
    }
}

int ngx_xapian_build_search_index(const char* directory, const char* language, const char* target) {
    WritableDatabase database(target, DB_CREATE_OR_OPEN);
    TermGenerator termGenerator;
    termGenerator.set_stemmer(Stem(language));
    xapian_traverse_index_directories(database, termGenerator, directory);
    return 0;
}

int ngx_xapian_search_search_index(const char* index, const char* language, const char* query, int max_results, xapian_chunk_callbackp chunkCallback, void* data) {
    Database database(index);
    QueryParser queryParser;
    queryParser.set_stemmer(Stem(language));
    queryParser.set_stemming_strategy(QueryParser::STEM_SOME);

    auto parsedQuery = queryParser.parse_query(query);
    Enquire inquiry(database);
    inquiry.set_query(parsedQuery);
    MSet docset = inquiry.get_mset(0, max_results);

    int total = 0;
    for (MSet::iterator it = docset.begin(); it != docset.end(); ++it) {
        auto str = it.get_document().get_data();
        chunkCallback(str.data(), str.size(), data);
    }
    return total;
}

// Should be free'd with 'free' after use.
int ngx_xapian_search_search_index_json(const char* index, const char* language, const char* query, int max_results, xapian_chunk_callbackp chunkCallback, void* data) {
    tuple<void*, void*, int, bool> values((void*)chunkCallback, (void*)data, 0, true);
    chunkCallback("{\"results\":[", sizeof("{\"results\":[")-1, data);
    get<2>(values) += sizeof("{\"results\":[")-1;
    ngx_xapian_search_search_index(index, language, query, max_results, +[](const char* chunk, unsigned int chunkSize, void* data){
        auto values = (tuple<void*, void*, int, bool>*)data;
        xapian_chunk_callbackp chunkCallback = (xapian_chunk_callbackp)get<0>(*values);
        if (get<3>(*values)) {
            chunkCallback(",", 1, get<1>(*values));
            get<3>(*values) = false;
            get<2>(*values) += 1;
        }
        chunkCallback(chunk, chunkSize, get<1>(*values));
        get<2>(*values) += chunkSize;
    }, &values);
    chunkCallback("]}", 2, data);
    get<2>(values) += 2;
    return get<2>(values);
}

