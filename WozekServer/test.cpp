#ifdef TESTRUN
#include "Everything.hpp"


void testDatabase()
{
	using namespace db;
	
	Database database;
	databaseManager.setDatabase(database);
	
	assert( databaseManager.addRecord<db::Database::Table::Host>(Host::Header{1, "Host nr 1"}) );
	assert( databaseManager.addRecord<db::Database::Table::Host>(Host::Header{3, "Host nr 3"}) );
	assert( databaseManager.addRecord<db::Database::Table::Host>(Host::Header{2, "Host nr 2"}) );
	assert( !databaseManager.addRecord<db::Database::Table::Host>(Host::Header{2, "Host nr 2"}) );
	assert( databaseManager.createAndAddRecord<db::Database::Table::Host>(Host::Header{2, "Host nr 2 new"}) );
	assert( !databaseManager.removeRecord<db::Database::Table::Host>(2424) );
	
	std::cout << '\n';
	showDatabase(database);
	
	assert( databaseManager.addRecord<db::Database::Table::Controller>(Controller::Header{2}) );
	assert( databaseManager.addRecord<db::Database::Table::Controller>(Controller::Header{3}) );
	assert( databaseManager.addRecord<db::Database::Table::Controller>(Controller::Header{1}) );
	
	std::cout << '\n';
	showDatabase(database);
	
	assert( databaseManager.removeRecord<db::Database::Table::Host>(1) );
	assert( !databaseManager.removeRecord<db::Database::Table::Host>(2424) );
	
	std::cout << '\n';
	showDatabase(database);
	
	
}

int main()
{
	testDatabase();
}

#endif // TESTRUN


