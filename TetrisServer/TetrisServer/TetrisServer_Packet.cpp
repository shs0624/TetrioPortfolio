#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "TetrisServer.h"

void TetrisServer::mpRESLogin(RefCountPointer& cPacket, BYTE status)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_TETRISLOGIN_RES_LOGIN;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << status;
}