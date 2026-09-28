# [서버 개발자] 성현식

## IOCP기반 테트리스 온라인 게임 서버

**`[프로젝트 개요]`**

IOCP 기반 네트워크 라이브러리를 활용해 구현한 1 vs 1 대전 테트리스 게임입니다.

유저의 흐름은 다음과 같은 순서로 로그인서버에서 인증 후, 게임 서버에 입장하여 게임을 진행합니다.

로그인 서버 : 회원가입 -> 로그인
게임 서버 : Redis 인증 -> 채팅(로비) 입장 -> 매칭 시도 -> 매칭 완료 후 게임 진행 -> 채팅(로비)로 복귀

메시지 처리는 IOCP 워커스레드가 처리하도록 했고, 게임 세션의 Update는 10개의 게임 틱 스레드가 500개의 세션을 초당 30회(30fps) 순회하며 진행하게 했습니다. 

이 때 메시지 처리와 게임 틱 스레드의 임계영역은 SRWLock으로 관리했습니다.

**`[프로젝트 실행 영상]`**

https://youtu.be/oWPrbJxXiPQ

**`[기간]`**

2026.07.16 ~ 2026.09.28

**`[스레드구조]`**
<details>
<summary>테트리스 서버 메시지 송수신 스레드 구조</summary>
<div markdown="1" style="padding-left: 15px;">
<img width="1283" height="464" alt="image" src="https://github.com/user-attachments/assets/2803e4e2-41dc-4b43-98de-9581d97e0f4e" />
</div>
</details>

<details>
<summary>테트리스 서버 게임 로직 스레드 구조</summary>
<div markdown="1" style="padding-left: 15px;">
<img width="1055" height="472" alt="image" src="https://github.com/user-attachments/assets/d1bc61f6-930d-4a2f-977a-8d10ba2ca86d" />
</div>
</details>

**`[개발 내용]`**

- 회원가입 과정에서 중복 체크를 Redis를 활용해 진행하여 중복체크 성공한 ID,닉네임의 경우 다른 유저가 사용하지 못하도록 구현
  - [로그인 서버 중복체크 Redis 활용](https://github.com/shs0624/TetrioPortfolio/blob/132b9dfd7b55b66025f5649c91862c083a4403af/TetrisLoginServer/TetrisLoginServer/TetrisLoginServer.cpp#L196-L316)
- 로그인 서버 -> 게임 서버의 세션 인증을 Redis를 활용해서 게임 서버가 추가로 DB에 접근하지 않도록 구현
  -  [게임 서버의 로그인과정](https://github.com/shs0624/TetrioPortfolio/blob/132b9dfd7b55b66025f5649c91862c083a4403af/TetrisServer/TetrisServer/TetrisServer_Message.cpp#L11)
- 게임 세션은 스레드마다 500개의 세션을 배열에서 할당받으며, 배열의 할당받은 원소들을 순회하며 Update 진행하여 게임 틱 스레드끼리 경합하지 않도록 구현
  - [GameTickThread 구현](https://github.com/shs0624/TetrioPortfolio/blob/132b9dfd7b55b66025f5649c91862c083a4403af/TetrisServer/TetrisServer/TetrisServer_GameLogic.cpp#L10-L47) 
  - [매칭 후 세션 세팅](https://github.com/shs0624/TetrioPortfolio/blob/132b9dfd7b55b66025f5649c91862c083a4403af/TetrisServer/TetrisServer/TetrisServer_GameLogic.cpp#L49)
- 컨텐츠는 서버 권위 구조로 구현하여 서버가 모든 연산을 하고, 결과만을 각 클라이언트에게 보내는 방식으로 구현
  - [게임 보드, 블록 위치 업데이트](https://github.com/shs0624/TetrioPortfolio/blob/132b9dfd7b55b66025f5649c91862c083a4403af/TetrisServer/TetrisServer/TetrisServer_GameLogic.cpp#L184)
  - [생성 예정 블록 리스트 구현](https://github.com/shs0624/TetrioPortfolio/blob/132b9dfd7b55b66025f5649c91862c083a4403af/TetrisServer/TetrisServer/TetrisServer_GameLogic.cpp#L242-L310)

**`[사용기술]`**

- Language : C++
