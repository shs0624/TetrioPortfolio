#pragma once
#define QUERY_MAXCLASSSIZE 80

enum enQueryType
{
	_enLogin
};

struct CDBPoolStruct
{
	char _chMemory[QUERY_MAXCLASSSIZE];
};

class IDBJob
{
public:
	virtual bool Exec(ostringstream& oss) = 0;
};

class CDBLogin : public IDBJob
{
public:
	bool Exec(ostringstream& oss)
	{
		// Äõ¸® »ý¼º
		oss.clear();

		oss << "SELECT * FROM accountdb.sessionkey WHERE accountno = '"
			<< _AccountNum << "';";

		return true;
	}

	_int64 _AccountNum;
	char _SessionKey[64];
};


