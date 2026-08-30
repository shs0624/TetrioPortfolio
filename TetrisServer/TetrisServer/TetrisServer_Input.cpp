#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "GameHeader.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"

void TetrisServer::MoveLeft(st_GameInfo* pGameInfo)
{
	if(pGameInfo->_DropX > 0)
		pGameInfo->_DropX -= 1;
}

void TetrisServer::MoveRight(st_GameInfo* pGameInfo)
{
	if (pGameInfo->_DropX < _iMaxX - 1)
		pGameInfo->_DropX += 1;
}

void TetrisServer::SoftDrop(st_GameInfo* pGameInfo)
{

}

void TetrisServer::HardDrop(st_GameInfo* pGameInfo)
{

}

void TetrisServer::Rotate(st_GameInfo* pGameInfo, bool clockwise)
{

}

void TetrisServer::Hold(st_GameInfo* pGameInfo, bool clockwise)
{

}