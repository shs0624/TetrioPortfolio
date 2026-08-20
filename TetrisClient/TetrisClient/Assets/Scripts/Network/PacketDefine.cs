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
    TETRIS_RES_MATCHING_SUCCESS       = 14,
    TETRIS_REQ_GAME_READY             = 15,
    TETRIS_RES_GAME_READY             = 16,
    TETRIS_SC_ACK_COUNTDOWN           = 17,
    TETRIS_REQ_HEARTBEAT              = 18,  // C -> S : 이 type을 word로 넣어야함
    TETRISLOGIN_REQ_HEARTBEAT         = 19,  // C -> S : 로그인 서버 콜드 타임아웃 방지용 하트비트. 이 type을 word로 넣어야함
}
