#pragma once
#include <cstdio>
#include <mutex>
#include <string>

// 로그 텍스트 파일로 남기는 용도의 라이브러리
// 각 줄 앞에 작성 시점을 남기며, 스레드 세이프한 방식 사용
class CTextLogger
{
public:
	explicit CTextLogger(const std::string& filePath);
	~CTextLogger();

	// 파일을 열 수 없으면 false. 다음 호출 때 다시 열기를 시도한다.
	bool Write(const std::string& message);

	// 닫은 뒤에 Write를 호출하면 파일을 다시 연다.
	void Close();

private:   
	bool OpenIfNeeded();

	std::string _filePath;
	FILE*       _file = nullptr;

	// 스레드 세이프용 뮤텍스
	std::mutex  _lock;
};
