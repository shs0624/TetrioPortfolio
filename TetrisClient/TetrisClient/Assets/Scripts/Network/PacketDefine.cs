/// <summary>
/// 서버 Protocol.h(en_PACKET_TYPE)의 선언 순서를 그대로 옮긴 값입니다.
/// en_PACKET_TYPE은 명시적 숫자 지정 없이 0부터 순서대로 매겨지는 enum이므로,
/// 서버 Protocol.h에 항목이 추가/삭제되면 이 파일도 같이(같은 순서로) 갱신해야 합니다.
/// </summary>
public enum PacketID : ushort
{
    // ── Login Server ──────────────────────────────────────────────────
    TETRISLOGIN_REQ_REGISTER          = 0,   // C -> S : ID[20] + Passwd[20] + Nickname[20] (ASCII)
    TETRISLOGIN_RES_REGISTER          = 1,   // S -> C : Status(1)   ※ 서버 수정 필요, 위 주석 참고
    TETRISLOGIN_REQ_DUPCHECK_ID       = 2,   // C -> S : ID[20] + Nickname[20] (둘 다 항상 채워서 보내야 함)
    TETRISLOGIN_REQ_DUPCHECK_NICKNAME = 3,   // C -> S : ID[20] + Nickname[20] (위와 동일 포맷)
    TETRISLOGIN_RES_DUPCHECK          = 4,   // S -> C : Status(1)   ※ 서버 수정 필요, 위 주석 참고
    TETRISLOGIN_REQ_LOGIN             = 5,   // C -> S : ID[20] + Passwd[20]
    TETRISLOGIN_RES_LOGIN             = 6,   // 실제로는 "게임서버"의 단순 로그인 응답(Status(1))에 쓰임

    // ── Game Server ───────────────────────────────────────────────────
    TETRIS_REQ_LOGIN                  = 7,   // C -> S : AccountNum(8, INT64) + SessionKey[64] (ASCII)
    TETRIS_RES_LOGIN                  = 8,   // 실제로는 "로그인서버"의 전체 로그인 응답에 쓰임
                                              // Status(1) + AccountNum(8) + GameIP[16](UTF-16) + GamePort(2) + SessionKey[64](UTF-16)

    TETRIS_ACK_CHAT_ENTER             = 9,
    TETRIS_ACK_CHAT_EXIT              = 10,
    TETRIS_REQ_CHAT_MESSAGE           = 11,
    TETRIS_REQ_MATCHING               = 12,
    TETRIS_RES_MATCHING               = 13,
    // C -> S : 본문 없음. 매칭 중일 때 매칭 버튼을 다시 누르면 전송.
    TETRIS_REQ_MATCHING_CANCEL         = 14,
    // S -> C : Status(1)
    TETRIS_RES_MATCHING_CANCEL         = 15,
    TETRIS_RES_MATCHING_SUCCESS       = 16,
    TETRIS_REQ_GAME_READY             = 17,
    TETRIS_RES_GAME_READY             = 18,
    TETRIS_SC_ACK_COUNTDOWN           = 19,

    // S -> C : NextBlockBag[5](WORD, enTetBlock) + MyBoard[20][10](BYTE) + OpponentBoard[20][10](BYTE)
    TETRIS_SC_ACK_GAME_BOARDUPDATE     = 20,
    // S -> C : IsSelf(1, BYTE, 서버 필드명은 IsOpponent지만 실제론 1=자기 자신 블록) + BlockType(1) + Rotate(1) + X(1, signed char) + Y(1, signed char)
    TETRIS_SC_ACK_GAME_BLOCKUPDATE     = 21,
    // S -> C : DamageCount(1, BYTE) — 아직 적용 안 된 대기 가비지 줄 수(받는 사람 자신 기준)
    TETRIS_ACK_GAME_DAMAGE             = 22,
    // C -> S : InputType(4, en_INPUT_TYPE) — AccountNum 없음(서버가 세션ID로 유저 식별)
    TETRIS_ACK_GAME_USERINPUT          = 23,
    // C -> S : InputType(4) — 서버 Protocol.h 주석이 USERINPUT과 동일하게 되어있음(서버 쪽 오탈자로 보임). 처리 로직은 아직 없음.
    TETRIS_ACK_GAME_RESULT              = 24,

    // C -> S : 본문 없음. 게임 결과 화면의 "로비로 돌아가기" 클릭 시 전송 — 서버가 EnterChat()을 재호출해 채팅방에 복귀시킨다.
    TETRIS_REQ_GAME_RETURNCHAT         = 25,
    // S -> C : Status(1)
    TETRIS_RES_GAME_RETURNCHAT         = 26,

    TETRIS_REQ_HEARTBEAT              = 27,  // C -> S : 이 type을 word로 넣어야함 (서버 en_PACKET_CS_CHAT_REQ_HEARTBEAT)

    // ⚠️ 서버 Protocol.h에 대응 항목이 없음 (기존부터 있던 불일치, 이번 작업 범위 밖).
    // 값 충돌만 피하도록 뒤로 밀어둠 — 로그인 하트비트 기능은 별도로 확인 필요.
    TETRISLOGIN_REQ_HEARTBEAT         = 28,
}
