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
	// 호출된 객체의 멤버변수를 변환하지 않음 -> 끝에 const
	// 쿼리 문자열 얻기. 변수는 ?로 표시
	virtual const char* GetQueryText() const = 0;
	virtual int GetParamCount() const = 0;
	virtual void BindParams(MYSQL_BIND* bind) = 0;
};

class CDBLogin : public IDBJob
{
public:
	const char* GetQueryText() const override
	{
		return "SELECT accountnum, passwd, nickname FROM accountdb WHERE id = ?";
	}

	int GetParamCount() const override
	{
		return 1;
	}

	void BindParams(MYSQL_BIND* bind) override
	{
		memset(bind, 0, sizeof(MYSQL_BIND) * GetParamCount());

		_IDLen = (unsigned long)strlen(_ID);

		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = _ID;
		bind[0].buffer_length = sizeof(_ID);
		bind[0].length = &_IDLen;
	}

	_int64 _AccountNum;
	char _ID[20];
	char _Passwd[20];
	//char _SessionKey[64];

	unsigned long _IDLen;
};

class CDBRegister_Check_ID : public IDBJob
{
public:
	const char* GetQueryText() const override
	{
		return "SELECT id FROM accountdb WHERE id = ?";
	}

	int GetParamCount() const override
	{
		return 1;
	}

	void BindParams(MYSQL_BIND* bind) override
	{
		memset(bind, 0, sizeof(MYSQL_BIND) * GetParamCount());

		_IDLen = (unsigned long)strlen(_ID);

		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = _ID;
		bind[0].buffer_length = sizeof(_ID);
		bind[0].length = &_IDLen;
	}

	_int64 _AccountNum;
	char _ID[20];
	unsigned long _IDLen;
};

class CDBRegister_Check_Nickname : public IDBJob
{
public:
	const char* GetQueryText() const override
	{
		return "SELECT nickname FROM accountdb WHERE nickname = ?";
	}

	int GetParamCount() const override
	{
		return 1;
	}

	void BindParams(MYSQL_BIND* bind) override
	{
		memset(bind, 0, sizeof(MYSQL_BIND) * GetParamCount());

		_NickLen = (unsigned long)strlen(_Nickname);

		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = _Nickname;
		bind[0].buffer_length = sizeof(_Nickname);
		bind[0].length = &_NickLen;
	}

	_int64 _AccountNum;
	char _Nickname[20];
	unsigned long _NickLen;
};

class CDBRegister_Insert : public IDBJob
{
public:
	const char* GetQueryText() const override
	{
		return "INSERT INTO accountdb (accountnum, id, passwd, nickname) VALUES (?, ?, ?, ?)";
	}

	int GetParamCount() const override
	{
		return 4;
	}

	void BindParams(MYSQL_BIND* bind) override
	{
		memset(bind, 0, sizeof(MYSQL_BIND) * GetParamCount());

		_IDLen = (unsigned long)strlen(_ID);
		_PasswdLen = (unsigned long)strlen(_Passwd);
		_NickLen = (unsigned long)strlen(_Nickname);

		bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
		bind[0].buffer = &_AccountNum;

		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = _ID;
		bind[1].buffer_length = sizeof(_ID);
		bind[1].length = &_IDLen;

		bind[2].buffer_type = MYSQL_TYPE_STRING;
		bind[2].buffer = _Passwd;
		bind[2].buffer_length = sizeof(_Passwd);
		bind[2].length = &_PasswdLen;

		bind[3].buffer_type = MYSQL_TYPE_STRING;
		bind[3].buffer = _Nickname;
		bind[3].buffer_length = sizeof(_Nickname);
		bind[3].length = &_NickLen;
	}

	_int64 _AccountNum;
	char _ID[20];
	char _Passwd[20];
	char _Nickname[20];

	unsigned long _IDLen;
	unsigned long _PasswdLen;
	unsigned long _NickLen;
};



