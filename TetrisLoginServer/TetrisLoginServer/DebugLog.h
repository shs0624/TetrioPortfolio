#pragma once

// 소켓 함수 오류 출력 후 종료
inline void err_quit(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%s] TCP Error Number : %d\n", msg, err);
	exit(1);
}

inline void err_display(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%s] TCP Error Number : %d\n", msg, err);
	return;
}