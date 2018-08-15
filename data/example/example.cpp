#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db_cxx.h"

// CREATE A DIRECTORY IN YOUR HOME DIR ~/cpsc5300/data before running this
const char *HOME = "cpsc5300/data";
const char *EXAMPLE = "example.db";
const unsigned int BLOCK_SZ = 4096;

int main(void) {
	std::cout << "Have you created a dir: ~/" << HOME  << "? (y/n) " << std::endl;
	std::string ans;
	std::cin >> ans;
	if( ans[0] != 'y')
		return 1;
	const char *home = std::getenv("HOME");
	std::string envdir = std::string(home) + "/" + HOME;

	DbEnv env(0U);
	env.set_message_stream(&std::cout);
	env.set_error_stream(&std::cerr);
	env.open(envdir.c_str(), DB_CREATE | DB_INIT_MPOOL, 0);

	Db db(&env, 0);
	db.set_message_stream(env.get_message_stream());
	db.set_error_stream(env.get_error_stream());
	db.set_re_len(BLOCK_SZ); // Set record length to 4K
	db.open(NULL, EXAMPLE, NULL, DB_RECNO, DB_CREATE | DB_TRUNCATE, 0644); // Erases anything already there

	char block[BLOCK_SZ];
	Dbt data(block, sizeof(block));
	int block_number;
	Dbt key(&block_number, sizeof(block_number));
	block_number = 1;
	strcpy(block, "hello!");
	db.put(NULL, &key, &data, 0);  // write block #1 to the database

	Dbt rdata;
	db.get(NULL, &key, &rdata, 0); // read block #1 from the database
	std::cout << "Read (block #" << block_number << "): '" << (char *)rdata.get_data() << "'";
	std::cout << " (expect 'hello!')" << std::endl;

	return EXIT_SUCCESS;
}

