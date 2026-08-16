/// <summary>
/// 서버 Protocol.h(en_PACKET_TYPE)의 선언 순서를 그대로 옮긴 값입니다.
/// en_PACKET_TYPE은 명시적 숫자 지정 없이 0부터 순서대로 매겨지는 enum이므로,
/// 서버 Protocol.h에 항목이 추가/삭제되면 이 파일도 같이(같은 순서로) 갱신해야 합니다.
///
/// ⚠️ 서버 실제 코드 확인 결과, 아래 두 항목은 Protocol.h 주석의 설명과
///    실제로 보내는 쪽 코드(태그)가 서로 반대로 되어 있습니다 (서버 버그로 추정).
///    클라이언트는 "실제로 서버가 보내는 값" 기준으로 맞춰뒀습니다.
///
///    - TETRISLOGIN_RES_LOGIN(6) : 주석상 "로그인서버의 전체 로그인 응답"이지만,
///      실제로는 TetrisServer_Packet.cpp::mpRESLogin() (게임서버의 단순 상태 응답)이 이 값을 씁니다.
///    - TETRIS_RES_LOGIN(8) : 주석상 "게임서버의 단순 응답"이지만,
///      실제로는 TetrisLoginServer_MakePacket.cpp::mpLoginRES() (로그인서버의 전체 응답,
///      GameIP/GamePort/SessionKey 포함)이 이 값을 씁니다.
///
///    서버 쪽을 문서대로 고치실 거면 이 파일과 Client.cs의 관련 주석도 같이 뒤집어야 합니다.
///
/// ⚠️ 또한 TetrisLoginServer_MakePacket.cpp의 mpRegisterRES() / mpDupcheckRES()는
///    현재 Protocol.h에 선언조차 안 된 en_PACKET_CS_TETRIS_RES_REGISTER를 참조하고 있어
///    로그인 서버가 지금 상태로는 빌드되지 않을 가능성이 높습니다.
///    TETRISLOGIN_RES_REGISTER / TETRISLOGIN_RES_DUPCHECK로 각각 나눠 고쳐야 합니다.
///    (자세한 내용은 대화 중 리뷰 참고 — 이 파일은 "서버가 고쳐졌을 때의 의도된 값"으로 맞춰뒀습니다.)
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
    CHAT_REQ_HEARTBEAT                = 18,
}
