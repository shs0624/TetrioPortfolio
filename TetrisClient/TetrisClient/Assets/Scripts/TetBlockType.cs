/// <summary>
/// 서버 UserSession.h의 enum enTetBlock을 그대로 옮긴 값입니다.
/// BoardUpdate/BlockUpdate 패킷의 블록 타입 바이트가 이 값과 1:1로 대응합니다.
/// </summary>
public enum TetBlockType : byte
{
    NoneBlock    = 0,
    IBlock       = 1,
    OBlock       = 2,
    TBlock       = 3,
    SBlock       = 4,
    ZBlock       = 5,
    JBlock       = 6,
    LBlock       = 7,
    GarbageBlock = 8,
}
