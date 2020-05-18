#include <iostream>
#include <xapian.h>


using namespace std;
using namespace Xapian;

void write() {

    WritableDatabase database("example", DB_CREATE_OR_OPEN);
    TermGenerator termGenerator;
    termGenerator.set_stemmer(Stem("en"));
    Document document;
    termGenerator.set_document(document);
    termGenerator.index_text("This is my test string. It's a test, you understand.");
    document.set_data("Data");
    document.add_boolean_term("MyTest");
    database.replace_document("MyTest", document);
}

void search() {
    Database database("example");
    QueryParser queryParser;
    queryParser.set_stemmer(Stem("en"));
    queryParser.set_stemming_strategy(QueryParser::STEM_SOME);

    auto query = queryParser.parse_query("test");
    Enquire inquiry(database);
    inquiry.set_query(query);
    MSet docset = inquiry.get_mset(0, 10);
    for (MSet::iterator it = docset.begin(); it != docset.end(); ++it) {
        fprintf(stderr, "WAT: %s", it.get_document().get_data().data());
    }
}

int main()
{
    write();
    search();
    return 0;
}
