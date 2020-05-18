#include <xapian.h>
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <regex>

#include "search.h"

using namespace std;
using namespace Xapian;
using namespace rapidjson;


bool xapian_index_file(WritableDatabase& database, TermGenerator& termGenerator, const string& path) {
    Document document;
    termGenerator.set_document(document);

    FILE* file = fopen(path.data(), "rb");
    if (!file)
        throw exception("Can't open file.");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    string buffer;
    buffer.resize(size);
    fread(const_cast<char*>(buffer.data()), sizeof(char), size, file);
    
    // Rather than using libXML2, just pump these into a regex, and print out the JSON. If it gets more complicated, start using libraries, but for now, this should do.
    string title;
    string description;
    static auto titleRegex = regex("<title>");
    static auto descriptionRegex = regex("<meta\s+.*name=\"description\" value=\"");
    cmatch match;
    if (regex_search(buffer, match, titleRegex)) {
        for (int i = match.position(); i < buffer.size(); ++i) {
            if (strncmp(&buffer[i], "</title>") == 0) {
                title = string(&buffer[match.position()], min(i - match.position(), 1024));
                break;
            }
        }
    }
    if (regex_search(buffer, match, descriptionRegex)) {
        for (int i = match.position(); i < buffer.size(); ++i) {
                if (buffer[i] == '"' && (i == 0 || buffer[i-1] != '\\')) {
                    description = string(&buffer[match.position()], min(i - match.position(), 1024));
                    break;
                }
        }
    }
    
    if (title.empty() || description.empty())
        return false;
        
    // Rather than including rapidJSON, just pump these strings in. If it proves problematic, we'll include the library.
    char outputBuffer[1024*4] = "{\"path\":\"";
    int offset = sizeof("{\"path\":\"") - 1;
    
    int copyToJson(char* dst, const char* str, int len) {
        char* target = dst;
        for (int i = 0; i < len; ++i) {
            if (str[i] == '"' && (i == 0 || str[i-1] != '\\'))
                *(target++) = '\';
            *(target++) = str[i];
        }
        return target - dst;
    }
    offset += copyToJson(&outputBuffer[offset], path, strlen(path));
    memcpy(&outputBuffer[offset], "\",\"title\":\"", sizeof("\",\"title\":\"")-1); offset += sizeof("\",\"title\":\"")-1;
    offset += copyToJson(&outputBuffer[offset], title.data(), title.size());
    memcpy(&outputBuffer[offset], "\",\"description\":\"", sizeof("\",\"description\":\"")-1); offset += sizeof("\",\"description\":\"")-1;
    offset += copyToJson(&outputBuffer[offset], description.data(), description.size());
    memcpy(&outputBuffer[offset], "\"}");
    
    termGenerator.index_text(buffer.data());
    document.set_data(sbuffer.GetString());
    document.add_boolean_term(path);
    database.replace_document(path, document);
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

int ngx_xapian_search_search_index(const char* index, const char* language, const char* query, int max_results, xapian_chunk_callback chunkCallback, void* data) {
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
int ngx_xapian_search_search_index_json(const char* index, const char* language, const char* query, int max_results, xapian_chunk_callback chunkCallback, void* data) {
    tuple<void*, void*, int, bool> values = { (void*)chunkCallback, (void*)data, 0, true };
    chunkCallback("{\"results\":[", sizeof("{\"results\":[")-1);
    get<2>(values) += sizeof("{\"results\":[")-1;
    ngx_xapian_search_search_index(index, language, query, max_results, +[](const char* chunk, unsigned int chunkSize, void* data){
        auto values = (tuple<void*, void*, int, bool>*)data;
        xapian_chunk_callbackp chunkCallback = (xapian_chunk_callbackp)get<0>(*values);
        if (get<3>(*values)) {
            chunkCallback(",", 1, get<1>(*values));
            get<3>(*values) = false;
            get<2>(*values) += 1;
        }
        get<2>(*values) += chunkCallback(chunk, chunkSize, get<1>(*values));
    }, &values);
    chunkCallback("]}", 2);
    get<2>(values) += 2;
    return total;
}

