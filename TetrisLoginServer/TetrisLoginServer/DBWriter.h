#pragma once
#pragma comment(lib,"libmysql.lib")
#include "Includes.h"
#include "mysql.h"
#include "errmsg.h"
#include "DBQueryProtocol.h"
#include "TLSMemoryPool.h"

namespace SHS
{
	class DBWriter
	{
	public:
		DBWriter() {};
		~DBWriter() {};

		void InitDBWriter(TLSMemoryPoolManager< CDBPoolStruct>* JobPool)
		{
			InitializeSRWLock(&_QueueLock);
			_hQEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

			_DBWriteThread = std::thread(&DBWriter::DBWriteThread, this);
		}

		bool ConnectMysql()
		{
			mysql_init(&_Conn);
			connection = mysql_real_connect(&_Conn, "127.0.0.1", "root", "shs0624@@", "newschema", 3306, (char*)NULL, CLIENT_MULTI_STATEMENTS);
			if (connection == NULL)
			{
				fprintf(stderr, "Mysql connection error : %s", mysql_error(&_Conn));
				return false;
			}

			_bConnected = true;
			return true;
		}

		// 작업을 큐에 인큐
		bool EnqueueJob(IDBJob* pJob)
		{
			if (_bConnected == false)
			{
				ConnectMysql();
			}

			AcquireSRWLockExclusive(&_QueueLock);
			_QueryQueue.push(pJob);
			ReleaseSRWLockExclusive(&_QueueLock);

			SetEvent(_hQEvent);
			return true;
		}

	private:
		void DBWriteThread()
		{
			MYSQL_RES* sql_result;
			MYSQL_ROW sql_row;
			int query_stat;

			IDBJob* pJob;
			while (1)
			{
				WaitForSingleObject(_hQEvent, INFINITE);

				AcquireSRWLockExclusive(&_QueueLock);
				pJob = _QueryQueue.front();
				_QueryQueue.pop();
				ReleaseSRWLockExclusive(&_QueueLock);


				int query_stat;
				std::ostringstream oss;

				pJob->Exec(oss);

				std::string sql = oss.str();

				// Select 쿼리문
				query_stat = mysql_query(&_Conn, sql.c_str());
				if (query_stat != 0) {
					printf("Mysql query error : %s", mysql_error(&_Conn));

					pJob->~IDBJob();
					_pJobPool->Free((CDBPoolStruct*)pJob);
					continue;
				}

				pJob->~IDBJob();
				_pJobPool->Free((CDBPoolStruct*)pJob);

				res = mysql_store_result(connection);
			}
		}


		MYSQL _Conn;
		bool _bConnected;

		HANDLE _hQEvent;
		SRWLOCK _QueueLock;
		std::queue<IDBJob*> _QueryQueue;
		std::thread _DBWriteThread;

		MYSQL* connection;
		MYSQL_RES* res;
		TLSMemoryPoolManager<CDBPoolStruct>* _pJobPool;
	};


	class DBWriterManager
	{
	public:
		DBWriterManager() {};
		~DBWriterManager() {};

		// 할당해가서 걔를 외부에서 placementNew
		LPVOID AllocJobAddress()
		{
			CDBPoolStruct* ptr = _JobPool.Alloc();

			return (LPVOID)ptr;
		}

		bool EnqueueJob(IDBJob* pJob, int iRoomNumber)
		{
			if (iRoomNumber == -1)
			{
				_SingleDBWriter.EnqueueJob(pJob);
			}
			else
			{
				int roomNum = iRoomNumber % _ithreadCount;
				_pDBWriterArr[roomNum].EnqueueJob(pJob);
			}
		}

		void InitDBWriterManager(int threadCount)
		{
			_SingleDBWriter.InitDBWriter(&_JobPool);
			_pDBWriterArr = (DBWriter*)malloc(sizeof(DBWriter) * threadCount);

			_ithreadCount = threadCount;
			// 연결이 아니라, 스레드를 생성해야함.
			for (int i = 0; i < threadCount; i++)
			{
				_pDBWriterArr[i].InitDBWriter(&_JobPool);
			}
		}
		
	private:
		int _ithreadCount;
		DBWriter _SingleDBWriter;
		DBWriter* _pDBWriterArr;

		static TLSMemoryPoolManager<CDBPoolStruct> _JobPool;
	};
}