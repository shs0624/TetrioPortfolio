#pragma once
#pragma comment(lib,"libmysql.lib")
#include "Includes.h"
#include "mysql.h"
#include "errmsg.h"
#include "DBQueryProtocol.h"

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
		bool SendQuery_SELECT(IDBJob* pJob)
		{
			int query_stat;
			std::ostringstream oss;

			pJob->Exec(oss);

			std::string sql = oss.str();

			// Select 쿼리문
			query_stat = mysql_query(connection, sql.c_str());
			if (query_stat != 0) {
				printf("Mysql query error : %s", mysql_error(&conn));
				return false;
			}

			pJob->~IDBJob();
			_JobPool.Free((CDBPoolStruct*)pJob);

			//res = mysql_store_result(connection);
			// 멀티문 결과 비우기
			return true;
		}

		// Fetch만 하는 것 결과는 따로 보기
		bool FetchQueryResult()
		{
			sql_row = mysql_fetch_row(res);
			if (sql_row == NULL)
				return false;

			return true;
		}

		bool StoreQueryResult()
		{
			int status = 0;
			res = mysql_store_result(connection);
			if (!res)
			{
				// INSERT/UPDATE는 NULL일 수 있지만 이 클래스는 SELECT 전용이니 무시
				// NULL이면 에러다. 서버 종료
				DebugBreak();
				return false;
			}

			return true;
		}

		void FreeQueryResult()
		{
			//int status = 0;
			//do {
			//	res = mysql_store_result(connection); // INSERT/COMMIT은 NULL이어도 OK
			//	if (res) 
			//		mysql_free_result(res);

			//	status = mysql_next_result(connection);
			//} while (status == 0);

			//if (status > 0) { // -1이 아닌 경우 에러
			//	printf("Mysql multi-result error : %s", mysql_error(&conn));
			//	return false;
			//}
			//return true;
			mysql_free_result(res);
		}

		// 전부 UTF-16으로?
		int GetInt(const char* columnName)
		{

		}

		_int64 GetInt64(const char* columnName)
		{

		}

		char* GetString(const char* columnName)
		{

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
		MYSQL_RES* res;
		MYSQL_ROW sql_row;

		static TLSMemoryPoolManager<CDBPoolStruct> _JobPool;
	};
}


