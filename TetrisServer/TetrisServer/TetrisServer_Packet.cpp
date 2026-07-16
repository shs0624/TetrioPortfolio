#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "TetrisServer.h"

void TetrisServer::mpRESLogin(RefCountPointer& cPacket, BYTE status, INT64 accountNum)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_TETRIS_RES_LOGIN;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << status;
	(**cPacket) << accountNum;
}