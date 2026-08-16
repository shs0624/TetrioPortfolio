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

			_stmtCache.clear();
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
		MYSQL_STMT* GetOrPrepareStmt(const char* queryText)
		{
			auto it = _stmtCache.find(queryText);
			if (it != _stmtCache.end()) return it->second;

			MYSQL_STMT* stmt = mysql_stmt_init(connection);
			if (mysql_stmt_prepare(stmt, queryText, (unsigned long)strlen(queryText)) != 0)
			{
				mysql_stmt_close(stmt);
				return nullptr;
			}
			_stmtCache[queryText] = stmt;
			return stmt;
		}

		void DBWriteThread()
		{
			IDBJob* pJob;
			while (1)
			{
				WaitForSingleObject(_hQEvent, INFINITE);

				while (true)
				{
					AcquireSRWLockExclusive(&_QueueLock);
					if (_QueryQueue.empty())
					{
						ReleaseSRWLockExclusive(&_QueueLock);
						break;
					}

					pJob = _QueryQueue.front();
					_QueryQueue.pop();
					ReleaseSRWLockExclusive(&_QueueLock);

					const char* queryText = pJob->GetQueryText();
					MYSQL_STMT* stmt = GetOrPrepareStmt(queryText);
					if (!stmt)
					{
						DebugBreak();
						_bConnected = false;

						pJob->~IDBJob();
						_pJobPool->Free((CDBPoolStruct*)pJob);
						break;
					}

					MYSQL_BIND paramBind[MYSQL_MAX_PARAM] = {};
					pJob->BindParams(paramBind);
					mysql_stmt_bind_param(stmt, paramBind);

					if (mysql_stmt_execute(stmt) != 0)
					{
						printf("Mysql query error : %s\n", mysql_stmt_error(stmt));
						mysql_stmt_close(stmt);
						_bConnected = false;

						pJob->~IDBJob();
						_pJobPool->Free((CDBPoolStruct*)pJob);
						continue;
					}

					pJob->~IDBJob();
					_pJobPool->Free((CDBPoolStruct*)pJob);

					//res = mysql_store_result(connection);

				}

			}
		}

		// stmt 준비해놓기 -> Query별로
		std::unordered_map<std::string, MYSQL_STMT*> _stmtCache;

		MYSQL _Conn;
		bool _bConnected;

		HANDLE _hQEvent;
		SRWLOCK _QueueLock;
		std::queue<IDBJob*> _QueryQueue;
		std::thread _DBWriteThread;

		MYSQL* connection;
		TLSMemoryPoolManager<CDBPoolStruct>* _pJobPool;
	};


	class DBWriterManager
	{
	public:
		DBWriterManager() {};
		~DBWriterManager() 
		{
		};

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
			//_pDBWriterArr = (DBWriter*)malloc(sizeof(DBWriter) * threadCount);
			_pDBWriterArr = new DBWriter[threadCount];

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