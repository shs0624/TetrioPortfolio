#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "TetrisLoginServer.h"
#include "Protocol.h"

void TetrisLoginServer::mpDupcheckRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status)
{
	(**cPacket) << (WORD)en_PACKET_CS_TETRIS_RES_REGISTER;
	(**cPacket) << status;
	(**cPacket) << accountNum;
}

void TetrisLoginServer::mpRegisterRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status)
{
	(**cPacket) << (WORD)en_PACKET_CS_TETRIS_RES_REGISTER;
	(**cPacket) << status;
	(**cPacket) << accountNum;
}

void TetrisLoginServer::mpLoginRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status, WCHAR* gameIP, USHORT gamePort, const WCHAR* sessionKey)
{
	(**cPacket) << (WORD)en_PACKET_CS_TETRIS_RES_LOGIN;
	(**cPacket) << status;
	(**cPacket) << accountNum;

	(*cPacket)->PutData((char*)gameIP, sizeof(WCHAR) * 16);
	(**cPacket) << gamePort;
	(*cPacket)->PutData((char*)sessionKey, sizeof(WCHAR) * 64);
}
