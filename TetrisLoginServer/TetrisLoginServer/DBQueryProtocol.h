#pragma once
#define QUERY_MAXCLASSSIZE 128

enum enLogin_Column
{
	_enAccountNum = 0,
	_enPasswd = 1,
	_enNickname = 2
};

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
		// 孽府 积己
		oss.clear();

		oss << "SELECT accountnum, passwd, nickname FROM accountdb WHERE id = '"
			<< _ID << "';";

		return true;
	}

	_int64 _AccountNum;
	char _ID[20];
	char _Passwd[20];
	char _SessionKey[64];
};

class CDBRegister_Check_ID : public IDBJob
{
public:
	bool Exec(ostringstream& oss)
	{
		// 孽府 积己
		oss.clear();

		oss << "SELECT id FROM accountdb WHERE id = '" << _ID << "';";

		return true;
	}

	_int64 _AccountNum;
	char _ID[20];
};

class CDBRegister_Check_Nickname : public IDBJob
{
public:
	bool Exec(ostringstream& oss)
	{
		// 孽府 积己
		oss.clear();

		oss << "SELECT nickname FROM accountdb WHERE nickname = '"
			<< _Nickname << "';";

		return true;
	}

	_int64 _AccountNum;
	char _Nickname[20];
};

class CDBRegister_Insert : public IDBJob
{
public:
	bool Exec(ostringstream& oss)
	{
		// 孽府 积己
		oss.clear();

		oss << "INSERT INTO accountdb (accountnum, id, passwd, nickname) VALUES ("
			<< _AccountNum << ", '"
			<< _ID << "', '"
			<< _Passwd << "', '"
			<< _Nickname << "');";

		return true;
	}

	_int64 _AccountNum;
	char _ID[20];
	char _Passwd[20];
	char _Nickname[20];
};



