#include "Includes.h"
#include "Util.h"
#include "Protocol.h"
#include "NetServer.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"

void TetrisServer::EnterChat(st_USER* userPtr)
{
	// 채팅 서버로의 입장
	InterlockedExchange((LONG*)&(userPtr->enServerState), en_SERVER_CHAT);

	// 채팅 벡터 내의 유저들 모두에게 채팅 메세지 전달
	RefCountPointer chatEnterPacket = RefCountPointer::MakeSharedPtr();
	(*chatEnterPacket)->Clear(sizeof(st_NetHeader));
	mpACKChatEnter(chatEnterPacket, userPtr->AccountNum, userPtr->NickName);
	MakePacketHeader(chatEnterPacket);

	AcquireSRWLockExclusive(&_ChatDataLock);
	// 자기 자신에게 기존 유저들의 Enter 메세지를 보내야할듯
	for (int i = 0; i < _ChatUserVec.size(); i++)
	{
		RefCountPointer roomUserPacket = RefCountPointer::MakeSharedPtr();
		(*roomUserPacket)->Clear(sizeof(st_NetHeader));
		mpACKChatEnter(roomUserPacket, _ChatUserVec[i]->AccountNum, _ChatUserVec[i]->NickName);
		MakePacketHeader(roomUserPacket);

		if (SendPacket_UniCast(userPtr->ulSessionID, roomUserPacket, false))
		{
			_pLog._dwChatEnterMessageTotal++;
			_pLog._dwChatEnterMessageTPS++;
		}
	}

	int idx = _ChatUserVec.size();
	_ChatUserVec.push_back(userPtr);
	_ChatUserIndexMap[userPtr->ulSessionID] = idx;

	for (int i = 0; i < _ChatUserVec.size(); i++)
	{
		chatEnterPacket.IncRefCount();
		if (SendPacket_UniCast(_ChatUserVec[i]->ulSessionID, chatEnterPacket, false))
		{
			_pLog._dwChatEnterMessageTotal++;
			_pLog._dwChatEnterMessageTPS++;
		}
	}

	// 자기 자신에게도 보냈으니 감소
	if (!chatEnterPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;

	_pLog._dwChatUserCount++;
	ReleaseSRWLockExclusive(&_ChatDataLock);
}

void TetrisServer::LeaveChat(ULONGLONG sessionID)
{
	AcquireSRWLockExclusive(&_ChatDataLock);

	auto itChat = _ChatUserIndexMap.find(sessionID);
	if (itChat != _ChatUserIndexMap.end())
	{
		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Clear(sizeof(st_NetHeader));

		st_USER* pExitUser = _ChatUserVec[(*itChat).second];

		mpACKChatExit(cPacket, pExitUser->AccountNum, pExitUser->NickName);
		MakePacketHeader(cPacket);

		// 자기 자신도 포함해서 RES를 보낼 것
		for (int i = 0; i < _ChatUserVec.size(); i++)
		{
			cPacket.IncRefCount();
			if (SendPacket_UniCast(_ChatUserVec[i]->ulSessionID, cPacket, false))
			{
				_pLog._dwChatLeaveMessageTotal++;
				_pLog._dwChatLeaveMessageTPS++;
			}
		}

		// 자신 포함해서 다 보냈으니 1을 줄이기
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;


		st_USER* pMovedUser = _ChatUserVec.back();
		int movedIdx = (*itChat).second;

		swap(_ChatUserVec.back(), _ChatUserVec[(*itChat).second]);
		_ChatUserVec.pop_back();
		_ChatUserIndexMap[pMovedUser->ulSessionID] = movedIdx;
		
		_pLog._dwChatUserCount--;
	}

	_ChatUserIndexMap.erase(sessionID);
	ReleaseSRWLockExclusive(&_ChatDataLock);
}