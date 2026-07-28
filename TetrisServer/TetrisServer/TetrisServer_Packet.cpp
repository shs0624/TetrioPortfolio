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

void TetrisServer::mpACKChatEnter(RefCountPointer& cPacket, INT64 accountNum, WCHAR* nickname)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_TETRIS_ACK_CHAT_ENTER;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << accountNum;

	(*cPacket)->PutData((char*)nickname, sizeof(WCHAR) * 20);
}

void TetrisServer::mpACKChatExit(RefCountPointer& cPacket, INT64 accountNum, WCHAR* nickname)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_TETRIS_ACK_CHAT_EXIT;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << accountNum;

	(*cPacket)->PutData((char*)nickname, sizeof(WCHAR) * 20);
}