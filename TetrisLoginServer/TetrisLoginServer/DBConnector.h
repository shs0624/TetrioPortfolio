#pragma once
#pragma comment(lib,"libmysql.lib")
#include "Includes.h"
#include "mysql.h"
#include "errmsg.h"
#include "DBQueryProtocol.h"

#define MYSQL_MAX_PARAM 4

namespace SHS
{
	class DBTLSConnector
	{
	public:
		// 다른 방법 - 해쉬키로 Write스레드를 할당
		// 어제 생각한 구조라면 RoomID를 전달하면 ,그 RoomID % 스레드개수로 해쉬해서 특정 방은 특정 스레드에 매핑
		// 그러면 그 방 안에서의 작업은 직렬화가 이뤄지고, 방이 많아져도 몰릴뿐 매핑은 정상적으로 이뤄진다.
		// 불가능한 점이 있을까? 방 단위의 직렬화만 가능해지는데,,,
		// 그 외의 세션 상태에서의 메세지는 별도의 단일 스레드에 몰아주자. (방에 들어가지 않은 상태의 Write)
		// 직렬화가 필요한 작업할 땐 writerThread가 처리하게 해야함.

		static DBTLSConnector* GetDBConnectorTLS()
		{
			thread_local DBTLSConnector client;
			thread_local bool connected = false;

			if (!connected) {
				mysql_thread_init();
				if (!client.InitConnection())
					DebugBreak();
				connected = true;
			}

			return &client;
		}

		// 할당해가서 걔를 외부에서 placementNew
		LPVOID AllocJobAddress()
		{
			CDBPoolStruct* ptr = _JobPool.Alloc();

			return (LPVOID)ptr;
		}

		// SELECT -> 직렬화 필요 x
		int SendQuery_SELECT(IDBJob* pJob)
		{
			MYSQL_STMT* stmt = mysql_stmt_init(connection);
			if (!stmt) 
			{ 
				DebugBreak(); 
				return false; 
			}

			const char* queryText = pJob->GetQueryText();
			if (mysql_stmt_prepare(stmt, queryText, (unsigned long)strlen(queryText)) != 0)
			{
				printf("Mysql prepare error : %s\n", mysql_stmt_error(stmt));
				mysql_stmt_close(stmt);
				return false;
			}

			MYSQL_BIND paramBind[MYSQL_MAX_PARAM] = {};
			pJob->BindParams(paramBind);
			mysql_stmt_bind_param(stmt, paramBind);

			if (mysql_stmt_execute(stmt) != 0)
			{
				printf("Mysql query error : %s\n", mysql_stmt_error(stmt));
				mysql_stmt_close(stmt);
				return false;
			}

			mysql_stmt_bind_result(stmt, _resultBind);

			_currentStmt = stmt;

			pJob->~IDBJob();
			_JobPool.Free((CDBPoolStruct*)pJob);

			return true;
		}

		int SendQuery_INSERT(IDBJob* pJob)
		{
			MYSQL_STMT* stmt = mysql_stmt_init(connection);
			if (!stmt) { DebugBreak(); return false; }

			const char* queryText = pJob->GetQueryText();
			if (mysql_stmt_prepare(stmt, queryText, (unsigned long)strlen(queryText)) != 0)
			{
				printf("Mysql prepare error : %s\n", mysql_stmt_error(stmt));
				int errNo = mysql_errno(connection);
				mysql_stmt_close(stmt);
				return errNo;
			}

			MYSQL_BIND paramBind[MYSQL_MAX_PARAM] = {};
			pJob->BindParams(paramBind);
			mysql_stmt_bind_param(stmt, paramBind);

			if (mysql_stmt_execute(stmt) != 0)
			{
				int errNo = mysql_errno(connection);   // 1062 (중복 키) 등은 그대로 감지 가능
				printf("Mysql query error : %s\n", mysql_stmt_error(stmt));
				mysql_stmt_close(stmt);
				return errNo;
			}

			mysql_stmt_close(stmt);

			pJob->~IDBJob();
			_JobPool.Free((CDBPoolStruct*)pJob);

			return true;
		}

		// Fetch만 하는 것 결과는 따로 보기
		bool FetchQueryResult()
		{
			int ret = mysql_stmt_fetch(_currentStmt);
			if (ret == MYSQL_DATA_TRUNCATED)
			{
				// 데이터가 잘려 도착한 경우
				DebugBreak();
				return false;
			}

			if (ret != 0)
				return false;

			return true;
		}

		bool StoreQueryResult()
		{
			if (mysql_stmt_store_result(_currentStmt) != 0)
			{
				DebugBreak();
				return false;
			}

			return true;
		}

		void FreeQueryResult()
		{
			mysql_stmt_free_result(_currentStmt);
			mysql_stmt_close(_currentStmt);
			_currentStmt = nullptr;
		}

		// 전부 UTF-16으로?
		int GetInt(const int enColumn)
		{
			return atoi(_resStr[enColumn]);
		}

		_int64 GetInt64(const int enColumn)
		{
			return _atoi64(_resStr[enColumn]);
		}

		char* GetString(const int enColumn)
		{
			return _resStr[enColumn];
		}

	private:
		DBTLSConnector() {};
		~DBTLSConnector() {};

		bool InitConnection()
		{
			MYSQL* sql = mysql_init(nullptr);
			if (sql == NULL)
			{
				//int n = mysql_errno(NULL);
				DebugBreak();
			}

			// MAX_RESULT_COLUMNS개의 문자열 슬롯을 한 번만 세팅
			memset(_resultBind, 0, sizeof(_resultBind));
			for (int i = 0; i < MYSQL_MAX_PARAM; ++i)
			{
				_resultBind[i].buffer_type = MYSQL_TYPE_STRING;
				_resultBind[i].buffer = _resStr[i];
				_resultBind[i].buffer_length = sizeof(_resStr[i]);
				_resultBind[i].length = &_resStrLen[i];
			}

			connection = mysql_real_connect(sql, "127.0.0.1", "root", "shs0624@@", "accountdb", 3306, (char*)NULL, CLIENT_MULTI_STATEMENTS);
			if (connection == NULL)
			{
				fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
				return false;
			}

			return true;
		}

		MYSQL conn;
		MYSQL* connection;

		MYSQL_STMT* _currentStmt;

		MYSQL_BIND		_resultBind[MYSQL_MAX_PARAM];
		char			_resStr[MYSQL_MAX_PARAM][128];
		unsigned long	_resStrLen[MYSQL_MAX_PARAM];

		static TLSMemoryPoolManager<CDBPoolStruct> _JobPool;
	};
}


