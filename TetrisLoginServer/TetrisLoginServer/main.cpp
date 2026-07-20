#include "Includes.h"
#include "NetServer.h"
#include "TetrisLoginServer.h"
#include "LogManager.h"

procademy::CCrashDump cCrashDump;

int main()
{
	srand(time(NULL));

    TetrisLoginServer* _loginServer = new TetrisLoginServer();
    _loginServer->InitLoginServer(INADDR_ANY, SERVERPORT, true, 20000);

	char ch;
	while (1)
	{
		// ÄÁÆ®·Ñ?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_loginServer->QuitServer();
			//break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("TetrisLogin_Profile.txt");
		}

	}
   
	return 0;
}