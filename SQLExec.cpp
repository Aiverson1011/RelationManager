/**
 * @file SQLExec.cpp - implementation of SQLExec class
 * @author Kevin Lundeen
 * @student Wonseok Seo, Amanda Iverson
 * @see "Seattle University, CPSC5300, Summer 2018"
 */
#include <algorithm>
#include "SQLExec.h"
#include "EvalPlan.h"
using namespace std;
using namespace hsql;

// define static data
Tables *SQLExec::tables = nullptr;
Indices *SQLExec::indices = nullptr;

// make query result be printable
ostream &operator<<(ostream &out, const QueryResult &qres) {
	if (qres.column_names != nullptr) {
		for (auto const &column_name : *qres.column_names)
			out << column_name << " ";
		out << endl << "+";
		for (unsigned int i = 0; i < qres.column_names->size(); i++)
			out << "----------+";
		out << endl;
		for (auto const &row : *qres.rows) {
			for (auto const &column_name : *qres.column_names) {
				Value value = row->at(column_name);
				switch (value.data_type) {
				case ColumnAttribute::INT:
					out << value.n;
					break;
				case ColumnAttribute::TEXT:
					out << "\"" << value.s << "\"";
					break;
				case ColumnAttribute::BOOLEAN:
					out << (value.n == 0 ? "false" : "true");
					break;
				default:
					out << "???";
				}
				out << " ";
			}
			out << endl;
		}
	}
	out << qres.message;
	return out;
}

// Clean column_names, column_attributes, row and rows if they exist
QueryResult::~QueryResult()
{
	if (column_names != nullptr)
		delete column_names;
	if (column_attributes != nullptr)
		delete column_attributes;
	if (rows != nullptr) {
		for (auto row : *rows)
			delete row;
		delete rows;
	}
}

/**
 * Execute given SQL statement by calling relevant
 * @param statement     SQL statment to execute
 * @return              QueryResult after execution or error message
 * @throw               DbRelation error
 */
QueryResult *SQLExec::execute(const SQLStatement *statement) throw(SQLExecError) {
	// if schema tables are null pointer, create new ones
	if (SQLExec::tables == nullptr) {
		SQLExec::tables = new Tables();
	}
	if (SQLExec::indices == nullptr) {
		SQLExec::indices = new Indices();
	}

	try {
		switch (statement->type()) {
		case kStmtCreate:
			return create((const CreateStatement *)statement);
		case kStmtDrop:
			return drop((const DropStatement *)statement);
		case kStmtShow:
			return show((const ShowStatement *)statement);
		case kStmtInsert:
			return insert((const InsertStatement *)statement);
		case kStmtDelete:
			return del((const DeleteStatement *)statement);
		case kStmtSelect:
			return select((const SelectStatement *)statement);
		default:
			return new QueryResult("not implemented");
		}
	}
	catch (DbRelationError &e) {
		throw SQLExecError(string("DbRelationError: ") + e.what());
	}
}

// helper function to take in a column and updates the column_name and
// column_attribute out variables
void SQLExec::column_definition(const ColumnDefinition *col, Identifier &column_name,
	ColumnAttribute &column_attribute) {
	// to hold column name
	column_name = col->name;
	//check column type, for now only INT or TEXT
	switch (col->type) {
	case ColumnDefinition::INT:
		column_attribute.set_data_type(ColumnAttribute::INT);
		break;
	case ColumnDefinition::TEXT:
		column_attribute.set_data_type(ColumnAttribute::TEXT);
		break;
	default:
		throw SQLExecError("Data Type Unsupported" + col->type);
	}
}

// helper function to get rows conditioned by where clause
ValueDict* SQLExec::get_where_conjunction(const Expr *where_clause) {
	// to hold rows
	ValueDict* rows = new ValueDict();
	// validate where clause type
	if (where_clause->type == kExprOperator) {
		// for now, we only accept AND for complicated where condition
		if (where_clause->opType == Expr::AND) {
			// recursively invoke for Expr
			ValueDict* sub_row = get_where_conjunction(where_clause->expr);
			rows->insert(sub_row->begin(), sub_row->end());
			sub_row = get_where_conjunction(where_clause->expr2);
			rows->insert(sub_row->begin(), sub_row->end());
			// in case of simple operator
		}
		else if (where_clause->opType == Expr::SIMPLE_OP) {
			Identifier column_name = where_clause->expr->name;
			Value value;
			// for now, only INT or STRING
			switch (where_clause->expr2->type) {
			case kExprLiteralString:
				value = Value(where_clause->expr2->name);
				break;
			case kExprLiteralInt:
				value = Value(where_clause->expr2->ival);
				break;
			default:
				throw DbRelationError("Unrecognized value!");
				break;
			}
			(*rows)[column_name] = value;
		}
		else
			throw DbRelationError("Unsupport opType!");
	}
	else
		throw DbRelationError("Unsupport type!");
	return rows;
}

// execute CREATE SQL statement
QueryResult *SQLExec::create(const CreateStatement *statement) {
	// check statement type
	switch (statement->type) {
	case CreateStatement::kTable:
		return create_table(statement);
	case CreateStatement::kIndex:
		return create_index(statement);
	default:
		return new QueryResult("Only CREATE INDEX and CREATE TABLE are implemented at this time");
	}
}

// The function to actually create the table. This function is also set to make sure
// that the statemetn is only for a table, and not anything else. 
QueryResult *SQLExec::create_table(const CreateStatement *statement) {
	//Double check if statement is CREATE
	if (statement->type != CreateStatement::kTable)
		return new QueryResult("Only handle CREATE TABLE");


	// get the name of the table to be created from the sql statement brought in. 
	Identifier tableName = statement->tableName;
	ValueDict row;
	// set the table name in the dictionary
	row["table_name"] = tableName;

	//update _tables schema
	Handle tHandle = SQLExec::tables->insert(&row);

	//Get columns to udate _columns schema
	ColumnNames colNames;
	Identifier colName;
	ColumnAttributes colAttrs;
	ColumnAttribute colAttr;

	//iterate through the columns that the statement has specified and set
	// the definitions and save them into the colNames vector and colAttr vectors
	for (ColumnDefinition* column : *statement->columns) {
		column_definition(column, colName, colAttr);
		colNames.push_back(colName);
		colAttrs.push_back(colAttr);
	}

	try {
		//update _columns schema
		Handles cHandles;
		DbRelation& cols = SQLExec::tables->get_table(Columns::TABLE_NAME);

		try {
			for (unsigned int i = 0; i < colNames.size(); i++) {
				row["column_name"] = colNames[i];
				row["data_type"] = Value(colAttrs[i].
					get_data_type() == ColumnAttribute::INT ? "INT" : "TEXT");
				cHandles.push_back(cols.insert(&row));
			}
			//Actually create the table (relation)
			DbRelation& table = SQLExec::tables->get_table(tableName);
			//Check which CREATE type
			if (statement->ifNotExists)
				table.create_if_not_exists();
			else
				table.create();
		}
		catch (exception& e) {
			// attempt to remove from _columns
			try {
				for (unsigned int i = 0; i < cHandles.size(); i++) {
					cols.del(cHandles.at(i));
				}
			}
			catch (...) {}
			throw;
		}
	}
	catch (exception& e) {
		try {
			//attempt to remove from _tables
			SQLExec::tables->del(tHandle);
		}
		catch (...) {}
		throw;
	}

	return new QueryResult("created " + tableName);
}

//M4 Create index method to create new table given query user provided
QueryResult *SQLExec::create_index(const CreateStatement *statement) {

	//Double check if statement is CREATE INDEX
	if (statement->type != CreateStatement::kIndex)
		return new QueryResult("Only handle CREATE INDEX");

	Identifier table_name = statement->tableName;
	ColumnNames column_names;
	Identifier index_name = statement->indexName;  //variable type might change
	Identifier index_type;
	bool is_unique;

	//Add to schema: _indices
	ValueDict row;

	row["table_name"] = table_name;


	try {
		index_type = statement->indexType;
	}
	catch (exception& e) {
		index_type = "BTREE";
	}


	if (index_type == "BTREE") {
		is_unique = true;
	}
	else {
		is_unique = false;
	}

	// setup and save the specific information about the row
	row["table_name"] = table_name;
	row["index_name"] = index_name;
	row["seq_in_index"] = 0;
	row["index_type"] = index_type;
	row["is_unique"] = is_unique;

	Handles iHandles;
	//Catching error when inserting each row to _indices schema table
	try {
		for (auto const& col : *statement->indexColumns) {
			row["seq_in_index"].n += 1;
			row["column_name"] = string(col);
			iHandles.push_back(SQLExec::indices->insert(&row));
		}


		DbIndex& index = SQLExec::indices->get_index(table_name, index_name);
		index.create();
	}
	catch (exception& e) {
		try {
			for (unsigned int i = 0; i < iHandles.size(); i++) {
				SQLExec::indices->del(iHandles.at(i));
			}
		}
		catch (...) {

		}

		throw;
	}

	return new QueryResult("created index " + index_name);

}


// execute DROP SQL statement
QueryResult *SQLExec::drop(const DropStatement *statement) {
	switch (statement->type) {
	case DropStatement::kTable:
		return drop_table(statement);
	case DropStatement::kIndex:
		return drop_index(statement);
	default:
		return new QueryResult("Only DROP TABLE and CREATE INDEX are implemented");
	}
}

// exectue DROP TABLE statement
QueryResult *SQLExec::drop_table(const DropStatement *statement) {
	Identifier table_name = statement->name;
	if (table_name == Tables::TABLE_NAME || table_name == Columns::TABLE_NAME || table_name == Indices::TABLE_NAME) {
		//return new QueryResult("unable to drop a schema table");
		throw SQLExecError("cannot drop a schema table");
	}

	ValueDict where;
	where["table_name"] = Value(table_name);

	// get the table
	DbRelation &table = SQLExec::tables->get_table(table_name);

	// remove indices - start
	// We need to check for all index associated with table_name
	IndexNames index_names = SQLExec::indices->get_index_names(table_name);
	for (auto const &index_name : index_names) {
		//need to remove all the rows associate with each of these IndexNames
		DbIndex &index = SQLExec::indices->get_index(table_name, index_name);
		index.drop();
	}
	Handles *handles = SQLExec::indices->select(&where);
	for (auto const &handle : *handles)
		SQLExec::indices->del(handle);
	delete handles;
	// remove indices - end

	// remove from _columns schema
	DbRelation &columns = SQLExec::tables->get_table(Columns::TABLE_NAME);
	handles = columns.select(&where);
	for (auto const &handle : *handles)
		columns.del(handle);
	delete handles;

	// remove table
	table.drop();
	SQLExec::tables->del(*SQLExec::tables->select(&where)->begin());

	return new QueryResult(string("dropped ") + table_name);
}


//M4 Drop Index
QueryResult *SQLExec::drop_index(const DropStatement *statement) {

	//Double check if statement is DROP Index
	if (statement->type != DropStatement::kIndex)
		return new QueryResult("Only handle DROP INDEX");


	// get the table name and index name from the sql statement being brought in
	Identifier table_name = statement->name;
	Identifier index_name = statement->indexName;
	// get the index from the database 
	DbIndex& index = SQLExec::indices->get_index(table_name, index_name);
	ValueDict where;
	// set the values 
	where["table_name"] = table_name;
	where["index_name"] = index_name;


	Handles* index_handles = SQLExec::indices->select(&where);
	index.drop();
	// iterate through the index handles and remove each indice
	for (unsigned int i = 0; i < index_handles->size(); i++) {
		SQLExec::indices->del(index_handles->at(i));
	}


	//Handle memory because select method returns the "new" pointer
	//declared in heap
	delete index_handles;

	return new QueryResult("dropped index " + index_name);
}

// execute SHOW SQL statement
QueryResult *SQLExec::show(const ShowStatement *statement) {
	switch (statement->type) {
	case ShowStatement::kTables:
		return show_tables();
	case ShowStatement::kColumns:
		return show_columns(statement);
	case ShowStatement::kIndex:
		return show_index(statement);
	default:
		throw SQLExecError("unrecognized SHOW type");
	}
}

// Function to show all of the tables in the database
QueryResult *SQLExec::show_tables() {
	ColumnNames* colNames = new ColumnNames;
	colNames->push_back("table_name");


	ColumnAttributes* colAttrs = new ColumnAttributes;
	colAttrs->push_back(ColumnAttribute(ColumnAttribute::TEXT));

	Handles* handles = SQLExec::tables->select();

	// -3 to discount schema table, schema columns, and schema indices
	u_long rowNum = handles->size() - 3;

	ValueDicts *rows = new ValueDicts;

	//Use project method to get all entries of table names
	for (unsigned int i = 0; i < handles->size(); i++) {
		ValueDict* row = SQLExec::tables->project(handles->at(i), colNames);
		Identifier tbName = row->at("table_name").s;

		//if table is not the schema table or column schema table, include in results
		if (tbName != Tables::TABLE_NAME && tbName != Columns::TABLE_NAME && tbName != Indices::TABLE_NAME)
			rows->push_back(row);
	}

	//Handle memory because select method returns the "new" pointer
	//declared in heap
	delete handles;

	return new QueryResult(colNames, colAttrs, rows,
		"successfully returned " + to_string(rowNum) + " rows");
}

// Function gets called when query requests to show the columns of a specific table
// Example query: show columns from goober
QueryResult *SQLExec::show_columns(const ShowStatement *statement) {
	DbRelation& cols = SQLExec::tables->get_table(Columns::TABLE_NAME);

	//Prepare column headers
	ColumnNames* colNames = new ColumnNames;
	colNames->push_back("table_name");
	colNames->push_back("column_name");
	colNames->push_back("data_type");

	ColumnAttributes* colAttrs = new ColumnAttributes;
	colAttrs->push_back(ColumnAttribute(ColumnAttribute::TEXT));

	ValueDict where;
	where["table_name"] = Value(statement->tableName);
	Handles* handles = cols.select(&where);
	u_long rowNum = handles->size();

	ValueDicts* rows = new ValueDicts;

	//Use project method to get all entries of column names of the table
	// iterate through the handles of the specific table targeted
	for (unsigned int i = 0; i < handles->size(); i++) {
		// get each column name and teh data type from the table.
		// an example would be to return "x (int)" "y(int)" "z(int)" on goober.  
		ValueDict* row = cols.project(handles->at(i), colNames);
		// add each row to the rows vector
		rows->push_back(row);
	}

	//Handle memory because select method returns the "new" pointer
	//declared in heap
	delete handles;

	//return the QR of all the information about the columns. This 
	// will get ouptutted to the terminal for the user to see the progress
	return new QueryResult(colNames, colAttrs, rows,
		" successfully returned " + to_string(rowNum) + " rows");
}


//M4 Show Index 

// example Command: CREATE index fx on goober(x,y)
QueryResult *SQLExec::show_index(const ShowStatement *statement) {

	Identifier table_name = statement->tableName;

	//Prepare column header
	ColumnNames* column_names = new ColumnNames;

	// table name: i.e. goober
	column_names->push_back("table_name");

	// index name: i.e. fx
	column_names->push_back("index_name");

	// column name: i.e. x
	column_names->push_back("column_name");

	// sequence : i.e. 1 (this was the first colmn specified in the paranthesis above)
	column_names->push_back("seq_in_index");

	// this will be a BTREE or a Hash. In example above this would be a BTREE (we are not implementing a hash)
	column_names->push_back("index_type");

	// make sure to flag that this index shouold be unique. in the above....we set it to true by default
	column_names->push_back("is_unique");


	ValueDict where;
	//set the table name in the VD of where
	where["table_name"] = Value(statement->tableName);

	// Get the blockids and record ides of the indices for that specific table specified. 
	Handles* handles = SQLExec::indices->select(&where);

	// the row numbers need to be equal to the number of handles we have
	u_long rowNum = handles->size();

	ValueDicts* rows = new ValueDicts;
	//Use project method to get all entries of column names of the table
	for (unsigned int i = 0; i < handles->size(); i++) {
		ValueDict* row = SQLExec::indices->project(handles->at(i), column_names);
		rows->push_back(row);
	}

	//Handle memory because select method returns the "new" pointer
	//declared in heap
	delete handles;

	//the Query result will return the name of the index's information 
	// along wtih the 2 rows we constructed (x and y)
	return new QueryResult(column_names, nullptr, rows,
		" successfully returned " + to_string(rowNum) + " rows");

}

// exectue INSERT SQL statement
QueryResult *SQLExec::insert(const InsertStatement *statement) {
	// get table name
	Identifier table_name = statement->tableName;
	// get table
	DbRelation &table = SQLExec::tables->get_table(table_name);

	// get column names
	ColumnNames input_column_names;
	if (statement->columns != NULL) {
		for (char* column : *statement->columns) {
			input_column_names.push_back(column);
		}
	}
	else {
		input_column_names = table.get_column_names();
	}

	// get values from statement
	vector<Value> records;
	for (auto const record : *statement->values) {
		switch (record->type) {
		case kExprLiteralString:
			records.push_back(Value(record->name));
			break;
		case kExprLiteralInt:
			records.push_back(Value(record->ival));
			break;
		default:
			throw DbRelationError("Unsupported Data type!");
		}
	}

	// to hold the number indices
	uint size;
	// to hold handle for column
	Handle column_handle;
	try {
		// to hold row
		ValueDict row;
		Identifier column_name;
		for (u_int16_t i = 0; i < input_column_names.size(); i++) {
			column_name = input_column_names.at(i);
			row[column_name] = records.at(i);
		}
		// insert row to table
		column_handle = table.insert(&row);

		// update index
		IndexNames index_names = SQLExec::indices->get_index_names(table_name);
		size = index_names.size();
		try {
			for (u_int16_t i = 0; i < index_names.size(); i++) {
				DbIndex &index = SQLExec::indices->get_index(table_name, index_names[i]);
				index.insert(column_handle);
			}
		}
		catch (exception& e) {
			for (u_int16_t i = 0; i < index_names.size(); i++) {
				DbIndex &index = SQLExec::indices->get_index(table_name, index_names[i]);
				index.del(column_handle);
			}
		}
	}
	catch (exception& e) {
		try {
			table.del(column_handle);
		}
		catch (...) {}
		throw;
	}
	string suffix = "";
	if (size != 0) {
		suffix = " and " + to_string(size) + " indices";
	}
	return new QueryResult("successfully inserted 1 row into " + table_name + suffix);
}

// exectue SELECT SQL statement
QueryResult *SQLExec::select(const SelectStatement *statement) {
	// get table name
	Identifier table_name = statement->fromTable->name;
	// get table
	DbRelation &table = SQLExec::tables->get_table(table_name);

	// to hold column names
	ColumnNames *query_names = new ColumnNames();
	vector<Expr*>* select_list = statement->selectList;
	if (select_list->at(0)->type == kExprStar) {
		select_list = nullptr;
		*query_names = table.get_column_names();
	}
	else {
		for (uint i = 0; i < select_list->size(); i++) {
			try {
				query_names->push_back(select_list->at(i)->name);
			}
			catch (exception& e) {
				throw DbRelationError("Unsupported column type!");
			}
		}
	}

	// create evaluation plan
	EvalPlan *plan = new EvalPlan(table);
	// in case of no where clause
	if (statement->whereClause != nullptr) {
		plan = new EvalPlan(get_where_conjunction(statement->whereClause), plan);
	}
	// in case of specified column/s selection
	if (select_list != nullptr) {
		plan = new EvalPlan(query_names, plan);
		// in case of '*' selection
	}
	else {
		plan = new EvalPlan(EvalPlan::ProjectAll, plan);
	}
	plan = plan->optimize();
	ValueDicts *rows = plan->evaluate();

	return new QueryResult(query_names, table.get_column_attributes(*query_names),
		rows, "successfully returned " + to_string(rows->size()) + " rows");
}

// exectue DELETE SQL statement
QueryResult *SQLExec::del(const DeleteStatement *statement) {
	// to thold table name
	Identifier table_name = statement->tableName;
	// to hold table
	DbRelation &table = SQLExec::tables->get_table(table_name);

	// create evaluation plan
	EvalPlan *plan = new EvalPlan(table);
	// in case of using where clause
	if (statement->expr != nullptr) {
		plan = new EvalPlan(get_where_conjunction(statement->expr), plan);
		// in case of no where clause
	}
	else {
		plan = plan->optimize();
	}

	// to hold evaluation pipeline
	EvalPipeline pipeline = plan->pipeline();
	// to hold index names
	IndexNames index_names = SQLExec::indices->get_index_names(table_name);
	// to hold hanles from piepleline
	Handles *handles = pipeline.second;
	// iterate to delete index from index table
	for (auto const &index_name : index_names) {
		DbIndex& index = SQLExec::indices->get_index(table_name, index_name);
		for (auto const &handle : *handles) {
			index.del(handle);
		}
	}
	// to hold suffix string statement for query result
	string suffix;
	// in case of no index deletion
	if (index_names.size() == 0)
		suffix = "";
	// in case of index/indices is/are deleted
	else
		suffix = " and from " + to_string(index_names.size()) + " indices";
	// delete from table
	for (auto const &handle : *handles) {
		pipeline.first->del(handle);
	}

	return new QueryResult("successfully deleted " + to_string(handles->size()) + " rows from " + table_name + suffix);
}
