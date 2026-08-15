#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "TetrisLoginServer.h"
#include "Protocol.h"

void TetrisLoginServer::mpDupcheckRES(RefCountPointer& cPacket, BYTE status)
{
	(**cPacket) << (WORD)en_PACKET_CS_TETRISLOGIN_RES_DUPCHECK;
	(**cPacket) << status;
}

void TetrisLoginServer::mpRegisterRES(RefCountPointer& cPacket, BYTE status)
{
	(**cPacket) << (WORD)en_PACKET_CS_TETRISLOGIN_RES_REGISTER;
	(**cPacket) << status;
}

void TetrisLoginServer::mpLoginRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status, WCHAR* gameIP, USHORT gamePort, const WCHAR* sessionKey)
{
	(**cPacket) << (WORD)en_PACKET_CS_TETRISLOGIN_RES_LOGIN;
	(**cPacket) << status;
	(**cPacket) << accountNum;

	if (status == dfTETRIS_LOGIN_OK)
	{
		(*cPacket)->PutData((char*)gameIP, sizeof(WCHAR) * 16);
		(**cPacket) << gamePort;
		(*cPacket)->PutData((char*)sessionKey, sizeof(WCHAR) * 64);
	}
}
