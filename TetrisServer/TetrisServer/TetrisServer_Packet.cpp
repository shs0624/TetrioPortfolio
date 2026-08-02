#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"

void TetrisServer::mpRESLogin(RefCountPointer& cPacket, BYTE status)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_TETRISLOGIN_RES_LOGIN;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << status;
}

void TetrisServer::mpRESChatMessage(RefCountPointer& cPacket, INT64 accountNum, WCHAR* nickname, WORD messageLen, WCHAR* message)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_TETRIS_REQ_CHAT_MESSAGE;

	(**cPacket) << accountNum;
	(*cPacket)->PutData((char*)nickname, sizeof(WCHAR) * 20);

	(**cPacket) << messageLen;
	(*cPacket)->PutData((char*)message, sizeof(WCHAR) * messageLen);
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

void TetrisServer::mpRESMatching(RefCountPointer& cPacket, BYTE status)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_TETRIS_RES_MATCHING;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << status;
}