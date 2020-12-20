#include <xapian.h>
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <regex>
#include <algorithm>
#include <exception>
#include <cstdarg>
#include <liquid/liquid.h>

#include "ngx_xapian_search.h"

using namespace std;
using namespace Xapian;

bool has_error = false;
char error_buffer[1024];

const char* ngx_xapian_get_error() {
    if (!has_error)
        return nullptr;
    has_error = false;
    return error_buffer;
}

void ngx_xapian_set_error(const char* error) {
    has_error = true;
    strncpy(error_buffer, error, sizeof(error_buffer));
}

void ngx_xapian_clear_error() {
    has_error = false;
}

const char* ngx_xapian_result_get_path(ngx_xapian_result_t* result, size_t* len) {
    int* lengths = (int*)result->pointer;
    int offset = sizeof(int)*4;
    *len = lengths[0];
    return &result->pointer[offset];
}
const char* ngx_xapian_result_get_title(ngx_xapian_result_t* result, size_t* len) {
    int* lengths = (int*)result->pointer;
    int offset = sizeof(int)*4 + lengths[0];
    *len = lengths[1];
    return &result->pointer[offset];
}
const char* ngx_xapian_result_get_description(ngx_xapian_result_t* result, size_t* len) {
    int* lengths = (int*)result->pointer;
    int offset = sizeof(int)*4 + lengths[0] + lengths[1];
    *len = lengths[2];
    return &result->pointer[offset];
}
const char* ngx_xapian_result_get_url(ngx_xapian_result_t* result, size_t* len) {
    int* lengths = (int*)result->pointer;
    int offset = sizeof(int)*4 + lengths[0] + lengths[1] + lengths[2];
    *len = lengths[3];
    return &result->pointer[offset];
}


struct SearchResult {
    string path;
    string title;
    string description;
    string url;

    string pack() const {
        string buffer;
        buffer.reserve(sizeof(int)*4 + path.size() + title.size() + description.size() + url.size());
        int size = path.size();
        buffer.append((char*)&size, sizeof(size));
        size = title.size();
        buffer.append((char*)&size, sizeof(size));
        size = description.size();
        buffer.append((char*)&size, sizeof(size));
        size = url.size();
        buffer.append((char*)&size, sizeof(size));
        buffer.append(path);
        buffer.append(title);
        buffer.append(description);
        buffer.append(url);
        return buffer;
    }
    static SearchResult unpack(const string& data) {
        int offset = 0;
        SearchResult result;
        int pathSize = *(int*)&data.data()[offset];
        offset += sizeof(int);
        int titleSize = *(int*)&data.data()[offset];
        offset += sizeof(int);
        int descriptionSize = *(int*)&data.data()[offset];
        offset += sizeof(int);
        int urlSize = *(int*)&data.data()[offset];
        offset += sizeof(int);
        result.path = data.substr(offset, pathSize);
        offset += pathSize;
        result.title = data.substr(offset, titleSize);
        offset += titleSize;
        result.description = data.substr(offset, descriptionSize);
        offset += descriptionSize;
        result.url = data.substr(offset, urlSize);
        return result;
    }
};

string extract_meta_attribute(const string& document, const string& attribute) {
    string regexBuffer = string("<meta\\s+.*name=[\"']") + attribute + "[\"']\\s+content\\s*=\\s*";
    auto metaRegex = regex(regexBuffer.data());
    smatch match;
    if (regex_search(document, match, metaRegex)) {
        unsigned int initial = match.position(0) + match.length(0);
        char delimiter = document[initial];
        if (delimiter == '"' || delimiter == '\'') {
            for (unsigned int i = initial+1; i < document.size(); ++i) {
                    if (document[i] == delimiter && (i == 0 || document[i-1] != '\\')) {
                        return string(&document[initial+1], min((int)(i - initial - 1), 1024));
                    }
            }
        }
    }
    return string();
}


string extract_link_attribute(const string& document, const string& attribute) {
    string regexBuffer = string("<link\\s+.*rel=[\"']") + attribute + "['\"]\\s+href\\s*=\\s*";
    auto metaRegex = regex(regexBuffer.data());
    smatch match;
    if (regex_search(document, match, metaRegex)) {
        unsigned int initial = match.position(0) + match.length(0);
        char delimiter = document[initial];
        if (delimiter == '"' || delimiter == '\'') {
            for (unsigned int i = initial+1; i < document.size(); ++i) {
                    if (document[i] == delimiter && (i == 0 || document[i-1] != '\\')) {
                        return string(&document[initial+1], min((int)(i - initial - 1), 1024));
                    }
            }
        }
    }
    return string();
}

struct CoreException : public exception {
    string internal;

    CoreException(const char* format, ...) {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        internal = buffer;
    }

    const char* what() const throw() { return internal.c_str(); }
};


// Allow for the "nointernalindex" class to be parsed out, as well as stripping all HTML tags.
// This should potentially use an HTML parsing library. For now, just do a hack job for a 'good-enough' use.
struct HTMLParser {
    enum class EStringParsingState {
        OPEN,
        SINGLE,
        DOUBLE
    };
    enum class ETagParsingState {
        OPEN,
        TAG,
        SCRIPT
    };

    const char* buffer;
    size_t offset;
    size_t lastCopied;
    EStringParsingState strState;
    ETagParsingState tagState;
    int noIndexDepth;

    string result;
    int tagStart;

    void copyUpTo(int place = -1) {
        if (place == -1)
            place = offset;
        result.append(buffer, lastCopied, place - lastCopied);
        result.push_back(' ');
        lastCopied = offset;
    }

    void attr(const char* attrName, int attrNameLength, const char* attrValue, int attrValueLength) {
        if (noIndexDepth == -1 && attrNameLength == 5 && strncmp(attrName, "class", 5) == 0) {
            static constexpr int strLength = sizeof("nointernalindex")-1;
            for (int i = 0; i < attrValueLength - strLength + 1; ++i) {
                if (strncmp(&attrValue[i], "nointernalindex", strLength) == 0 &&
                    ((i == attrValueLength - strLength) || isblank(attrValue[i+strLength+1])) &&
                    (i == 0 || isblank(attrValue[i-1]))
                ) {
                    noIndexDepth = 0;
                    break;
                }
            }
        }
    }

    void startOpenTag(const char* tag, int tagLength, int start) {
        if (noIndexDepth == -1)
            copyUpTo(start);
        else
            ++noIndexDepth;
    }

    void finishOpenTag() {
        lastCopied = offset+1;
    }

    void startCloseTag(const char* tag, int tagLength, int start) {
        if (noIndexDepth == -1)
            copyUpTo(start);
    }

    void finishCloseTag() {
        lastCopied = offset+1;
        if (noIndexDepth >= 0)
            --noIndexDepth;
    }

    string parse(const char* buffer, size_t size) {
        int tagStart;
        this->buffer = buffer;
        result.reserve(size);
        strState = EStringParsingState::OPEN;
        tagState = ETagParsingState::OPEN;
        lastCopied = 0;
        noIndexDepth = -1;
        int attrNameStart;
        int attrNameEnd;
        int attrValueStart;
        int tagNameStart;
        bool openTag;
        for (offset = 0; offset < size; ++offset) {
            char ch = buffer[offset];
            switch (strState) {
                case EStringParsingState::OPEN:
                    switch (tagState) {
                        case ETagParsingState::OPEN:
                            if (ch == '<') {
                                tagStart = offset;
                                attrNameStart = -1;
                                attrValueStart = -1;
                                tagNameStart = -1;
                                tagState = ETagParsingState::TAG;
                                openTag = true;
                            }
                        break;
                        case ETagParsingState::SCRIPT:
                            if (offset < size - 9 && strncmp(&buffer[offset], "</script>", 9) == 0) {
                                startCloseTag(&buffer[offset+2], 6, offset);
                                tagState = ETagParsingState::OPEN;
                                finishCloseTag();
                            }
                        break;
                        case ETagParsingState::TAG:
                            switch (ch) {
                                case '=':
                                    if (attrNameStart != -1) {
                                        attrNameEnd = offset;
                                        attrValueStart = offset+1;
                                    }
                                break;
                                case '\'':
                                    strState = EStringParsingState::SINGLE;
                                break;
                                case '"':
                                    strState = EStringParsingState::DOUBLE;
                                break;
                                case '>':
                                case '\t':
                                case '\n':
                                case ' ':
                                    if (attrNameStart == -1) {
                                        if (tagNameStart != -1) {
                                            if (buffer[tagStart+1] != '/')
                                                startOpenTag(&buffer[tagNameStart], offset - tagNameStart, tagStart);
                                            else
                                                startCloseTag(&buffer[tagNameStart], offset - tagNameStart, tagStart);
                                            openTag = false;
                                        }
                                    } else {
                                        int attrValueEnd;
                                        for (; buffer[attrValueStart] == '"' || buffer[attrValueStart] == '\''; ++attrValueStart);
                                        for (attrValueEnd = offset-1; buffer[attrValueEnd] == '"' || buffer[attrValueEnd] == '\''; --attrValueEnd);
                                        attr(&buffer[attrNameStart], attrNameEnd - attrNameStart, &buffer[attrValueStart], attrValueEnd - attrValueStart + 1);
                                    }
                                    if (ch == '>') {
                                        if (buffer[tagStart+1] == '/') {
                                            finishCloseTag();
                                            tagState = ETagParsingState::OPEN;
                                        } else {
                                            finishOpenTag();
                                            if (strncmp(&buffer[tagNameStart], "script", 6) == 0) {
                                                tagState = ETagParsingState::SCRIPT;
                                            } else {
                                                tagState = ETagParsingState::OPEN;
                                            }
                                        }
                                    }
                                break;
                                default:
                                    if (tagNameStart == -1) {
                                        tagNameStart = offset;
                                    } else if (!openTag && attrNameStart == -1) {
                                        attrNameStart = offset;
                                    }
                                break;
                            }
                        break;
                    }
                break;
                case EStringParsingState::SINGLE:
                    if (buffer[offset-1] != '\\' && ch == '\'') {
                        strState = EStringParsingState::OPEN;
                    }
                break;
                case EStringParsingState::DOUBLE:
                    if (buffer[offset-1] != '\\' && ch == '"')
                        strState = EStringParsingState::OPEN;
                break;
            }
        }
        copyUpTo();
        return move(result);
    }
};

bool xapian_index_file(WritableDatabase& database, TermGenerator& termGenerator, const string& path) {
    Document document;
    termGenerator.set_document(document);

    FILE* file = fopen(path.data(), "rb");
    if (!file)
        throw CoreException("Can't open file %s.", path.data());
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    string buffer;
    buffer.resize(size);
    if (fread(const_cast<char*>(buffer.data()), sizeof(char), size, file) != size) {
        throw "Can't read whole file.";
    }

    // Rather than using libXML2, just pump these into a regex, and print out the JSON. If it gets more complicated, start using libraries, but for now, this should do.
    SearchResult result;
    result.path = path;
    result.description = extract_meta_attribute(buffer, "description");
    string keywords = extract_meta_attribute(buffer, "keywords");
    string robots = extract_meta_attribute(buffer, "robot");
    string language = extract_meta_attribute(buffer, "language");
    result.url = extract_link_attribute(buffer, "canonical");
    static auto titleRegex = regex("<\\s*title\\s*>");
    smatch match;
    if (regex_search(buffer, match, titleRegex)) {
        for (unsigned int i = match.position(); i < buffer.size(); ++i) {
            if (strncmp(&buffer[i], "</title>", sizeof("</title>")-1) == 0) {
                unsigned int initial = match.position(0) + match.length(0);
                result.title = string(&buffer[initial], min((int)(i - initial), 1024));
                break;
            }
        }
    }

    if (result.title.empty() || result.description.empty() || robots.find("nointernalindex") != string::npos)
        return false;


    termGenerator.index_text(result.title.data(), 10);
    termGenerator.increase_termpos();
    termGenerator.index_text(keywords.data(), 3);
    termGenerator.increase_termpos();
    termGenerator.index_text(result.description.data(), 3);
    termGenerator.increase_termpos();

    HTMLParser parser;
    string text = parser.parse(buffer.data(), buffer.size());

    termGenerator.index_text(text.c_str());
    termGenerator.increase_termpos();

    document.set_data(result.pack());
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
                if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
                    strcpy(path, directory);
                    strcat(path, "/");
                    strcat(path, dp->d_name);
                    xapian_traverse_index_directories(database, termGenerator, path);
                }
            } break;
            case DT_REG:
                if (strncmp(&dp->d_name[strlen(dp->d_name)-5], ".html", 5) == 0) {
                    strcpy(path, directory);
                    strcat(path, "/");
                    strcat(path, dp->d_name);
                    xapian_index_file(database, termGenerator, path);
                }
            break;
        }
    }
}

int ngx_xapian_build_index(const char* directory, const char* language, const char* target) {
    try {
        WritableDatabase database(target, DB_CREATE_OR_OVERWRITE);
        TermGenerator termGenerator;
        termGenerator.set_stemmer(Stem(language));
        xapian_traverse_index_directories(database, termGenerator, directory);
    } catch (Xapian::Error& e) {
        ngx_xapian_set_error(e.get_msg().data());
        return -1;
    } catch (std::exception& e) {
        ngx_xapian_set_error(e.what());
        return -1;
    } catch (...) {
        ngx_xapian_set_error("Unknown error");
        return -1;
    }
    return 0;
}

int ngx_xapian_search_index(const char* index, const char* language, const char* query, int max_results, ngx_xapian_result_callbackp resultCallback, void* data) {
    int total = -1;
    try {
        Database database(index);
        QueryParser queryParser;
        queryParser.set_stemmer(Stem(language));
        queryParser.set_stemming_strategy(QueryParser::STEM_SOME);

        auto parsedQuery = queryParser.parse_query(query);
        Enquire inquiry(database);
        inquiry.set_query(parsedQuery);
        MSet docset = inquiry.get_mset(0, max_results);

        total = 0;
        for (MSet::iterator it = docset.begin(); it != docset.end(); ++it) {
            auto str = it.get_document().get_data();
            resultCallback({ str.data(), str.size() }, data);
            ++total;
        }
    } catch (Xapian::Error& e) {
        ngx_xapian_set_error(e.get_msg().data());
        return -1;
    } catch (std::exception& e) {
        ngx_xapian_set_error(e.what());
        return -1;
    } catch (...) {
        ngx_xapian_set_error("Unknown error");
        return -1;
    }
    return total;
}

int copyToJson(char* dst, const char* str, int len) {
    char* target = dst;
    for (int i = 0; i < len; ++i) {
        if (str[i] == '"' && (i == 0 || str[i-1] != '\\'))
            *(target++) = '\\';
        *(target++) = str[i];
    }
    return target - dst;
}
int copyToJsonField(char* dst, const char* name, const char* str, int len) {
    char* target = dst;
    int name_length = strlen(name);
    *(target++) = '"';
    target = (char*)memcpy(target, name, name_length) + name_length;
    *(target++) = '"';
    *(target++) = ':';
    *(target++) = '"';
    target += copyToJson(target, str, len);
    *(target++) = '"';
    return target - dst;
};

Liquid::Context& ngx_xapian_get_liquid_context() {
    static bool init = false;
    static Liquid::Context context;
    if (!init) {
        init = true;
        Liquid::StandardDialect::implementPermissive(context);
        Liquid::WebDialect::implement(context);
    }
    return context;
}

Liquid::Renderer& ngx_xapian_get_renderer() {
    static Liquid::Renderer renderer(ngx_xapian_get_liquid_context(), Liquid::CPPVariableResolver());
    return renderer;
}

void* ngx_xapian_parse_template(const char* buffer, int size) {
    Liquid::Parser parser(ngx_xapian_get_liquid_context());
    Liquid::Node* node = nullptr;
    try {
        node = new Liquid::Node(move(parser.parse(buffer, size)));
    } catch (const Liquid::Parser::Exception& e) {
        ngx_xapian_set_error(e.what());
    }
    return node;
}

void ngx_xapian_free_template(void* tmpl) {
    delete (Liquid::Node*)tmpl;
}

int ngx_xapian_search_template(const char* index, const char* language, const char* query, int max_results, void* tmpl, ngx_xapian_chunk_callbackp chunkCallback, void* data) {
    Liquid::CPPVariable hash, search, results;
    int resultCount = ngx_xapian_search_index(index, language, query, max_results, +[](ngx_xapian_result_t result, void* data){
        Liquid::CPPVariable* results = (Liquid::CPPVariable*)data;
        std::unique_ptr<Liquid::CPPVariable> cppResult = std::make_unique<Liquid::CPPVariable>();
        size_t titleLength;
        const char* title = ngx_xapian_result_get_title(&result, &titleLength);
        size_t descriptionLength;
        const char* description = ngx_xapian_result_get_description(&result, &descriptionLength);
        size_t urlLength;
        const char* url = ngx_xapian_result_get_url(&result, &urlLength);
        (*cppResult.get())["url"] = string(url, urlLength);
        (*cppResult.get())["title"] = string(title, titleLength);
        (*cppResult.get())["description"] = string(description, descriptionLength);
        results->pushBack(move(cppResult));
    }, &results);
    search["results"] = move(results);
    hash["search"] = move(search);
    hash["terms"] = string(query);
    Liquid::Renderer& renderer = ngx_xapian_get_renderer();
    std::string result = renderer.render(*(Liquid::Node*)tmpl, hash);
    chunkCallback(result.data(), result.size(), data);
    return resultCount;
}

// Should be free'd with 'free' after use.
int ngx_xapian_search_index_json(const char* index, const char* language, const char* query, int max_results, ngx_xapian_chunk_callbackp chunkCallback, void* data) {
    tuple<void*, void*, int, bool> values((void*)chunkCallback, (void*)data, 0, true);
    chunkCallback("{\"results\":[", sizeof("{\"results\":[")-1, data);
    get<2>(values) += sizeof("{\"results\":[")-1;
    int total = ngx_xapian_search_index(index, language, query, max_results, +[](ngx_xapian_result_t result, void* data){
        auto values = (tuple<void*, void*, int, bool>*)data;
        ngx_xapian_chunk_callbackp chunkCallback = (ngx_xapian_chunk_callbackp)get<0>(*values);
        if (!get<3>(*values)) {
            chunkCallback(",", 1, get<1>(*values));
            get<2>(*values) += 1;
        } else {
            get<3>(*values) = false;
        }

        // Rather than including rapidJSON, just pump these strings in. If it proves problematic, we'll include the library.
        char outputBuffer[1024*4] = "{";
        unsigned int offset = 1;

        size_t len;
        const char* buf = ngx_xapian_result_get_path(&result, &len);
        offset += copyToJsonField(&outputBuffer[offset], "path", buf, len);
        outputBuffer[offset++] = ',';
        buf = ngx_xapian_result_get_title(&result, &len);
        offset += copyToJsonField(&outputBuffer[offset], "title", buf, len);
        outputBuffer[offset++] = ',';
        buf = ngx_xapian_result_get_description(&result, &len);
        offset += copyToJsonField(&outputBuffer[offset], "description", buf, len);
        buf = ngx_xapian_result_get_url(&result, &len);
        if (len > 0) {
            outputBuffer[offset++] = ',';
            offset += copyToJsonField(&outputBuffer[offset], "url", buf, len);
        }
        outputBuffer[offset++] = '}';
        outputBuffer[offset] = 0;


        chunkCallback(outputBuffer, offset, get<1>(*values));
        get<2>(*values) += offset;
    }, &values);
    if (total < 0)
        return total;
    chunkCallback("]}", 2, data);
    get<2>(values) += 2;
    return get<2>(values);
}

