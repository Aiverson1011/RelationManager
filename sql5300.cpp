
/**
 * @filename: sql5300.cpp - main entry for the relatio nmanager's SQL shell
 * @author: Wonseok Seo, Amanda Seo - Advised from Lundeen K. @ SU
 * @version: Milestone 5
 * @see 'Seattle University, cpscp5300, Summer 2018'
 */

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <string>
#include <cassert>
#include <algorithm>
#include "db_cxx.h"
#include "SQLParser.h"
#include "ParseTreeToString.h"
#include "SQLExec.h"
#include "btree.h"

using namespace std;
using namespace hsql;

DbEnv *_DB_ENV;

/**
 * Shell class for interface between user and database
 */
class Shell {
public:
    // to hold user intend for continuity
    bool Continue;
    const string cmdHeader = "SQL> ";
    const string Quit = "QUIT";
    const string Test = "TEST";

    string sqlStatement;

    Shell() {
        //Inital instruction
        cout << "Type quit to end" << endl;
        this->Continue = true;

        //SQL command "SQL>"
        while (Continue) {
            // get user's input
            cout << cmdHeader;
            getline(cin, sqlStatement);
            // check if user wants to quit
            string tmp = sqlStatement;
            // transform user sql input to upper case
            transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);
            if (tmp == Quit) {
                Continue = false;
                break;
            } else if (tmp == Test) {
                cout << "test_heap_storage: "
                     << (test_heap_storage() ? "ok" : "failed") << endl;
                cout << "test btree: "
                     << (test_btree() ? "ok" : "failed") << endl;
                continue;
            }

            //Parse sql statement
            SQLParserResult *parseStatement = SQLParser::parseSQLString(sqlStatement);

            //Check if the statement is valid. If yes, print & execute
            //Otherwise, display error msg
            if (parseStatement->isValid()) {
                for (uint i = 0; i < parseStatement->size(); i++) {
                    const SQLStatement *statement = parseStatement->getStatement(i);
                    try {
                        cout << ParseTreeToString::statement(statement) << endl;
                        QueryResult *result = SQLExec::execute(statement);
                        cout << *result << endl;
                        delete result;
                    } catch (SQLExecError &e) {
                        cout << "Error: " << e.what() << endl;
                    }
                }
            } else {
                cout << "Invalid statement: " << sqlStatement << endl;
                cout << parseStatement->errorMsg() << endl;
            }
            delete parseStatement;
        }
    }
};

// main functino of the SQL database management program  project
int main(int argc, char *argv[]) {
    // check for command line input
    if (argc <= 1) {
        fprintf(stderr, "This program requires command line parameters\n");
        return 1;
    }

    // store command line argument as the path to the directory
    char* pathToDir = argv[1];

    // display directory path
    cout << "(sql5300: running with database environment at " << pathToDir
         << ")" << std::endl;

    //  open database environment
    DbEnv *myEnv = new DbEnv(0u);
    myEnv->set_message_stream(&cout);
    myEnv->set_error_stream(&cerr);
    try {
        myEnv->open(pathToDir, DB_CREATE | DB_INIT_MPOOL, 0);
    } catch (DbException &e) {
        cerr << "(sql5300: " << e.what() << ")" << endl;
        exit(1);
    }
    _DB_ENV = myEnv;

    // initialize schema tables
    initialize_schema_tables();

    // run SQL shell until user put 'quit' command
    Shell run;
    return 0;
}
