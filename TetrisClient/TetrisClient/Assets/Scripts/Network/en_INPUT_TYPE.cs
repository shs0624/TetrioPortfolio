/// <summary>
/// 서버 Protocol.h의 enum en_INPUT_TYPE을 이름/값 그대로 옮긴 것입니다.
/// TETRIS_ACK_GAME_USERINPUT 패킷의 DWORD InputType 필드에 그대로 실어 보냅니다.
/// </summary>
public enum en_INPUT_TYPE : uint
{
    en_INPUT_None = 0,
    en_INPUT_LEFT,
    en_INPUT_RIGHT,
    en_INPUT_SOFTDROP,
    en_INPUT_HARDDROP,
    en_INPUT_ROTATE_CLOCKWISE,
    en_INPUT_ROTATE_COUNTERCLOCKWISE,
    en_INPUT_HOLD,
}
