using UnityEngine;

public enum TetrominoType { I = 0, O = 1, T = 2, S = 3, Z = 4, J = 5, L = 6 }

/// <summary>Pure data: board constants, cell shapes, SRS wall-kick tables.</summary>
public static class TetrominoData
{
    public const int BoardWidth  = 10;
    public const int BoardHeight = 20;

    // Cell offsets from pivot, spawn orientation (state 0)
    public static readonly Vector2Int[][] Cells =
    {
        new[] { new Vector2Int(-1,0), new Vector2Int(0,0), new Vector2Int(1,0), new Vector2Int(2,0) }, // I
        new[] { new Vector2Int(0,0),  new Vector2Int(1,0), new Vector2Int(0,1), new Vector2Int(1,1) }, // O
        new[] { new Vector2Int(-1,0), new Vector2Int(0,0), new Vector2Int(1,0), new Vector2Int(0,1) }, // T
        new[] { new Vector2Int(-1,0), new Vector2Int(0,0), new Vector2Int(0,1), new Vector2Int(1,1) }, // S
        new[] { new Vector2Int(0,0),  new Vector2Int(1,0), new Vector2Int(-1,1),new Vector2Int(0,1) }, // Z
        new[] { new Vector2Int(-1,0), new Vector2Int(0,0), new Vector2Int(1,0), new Vector2Int(-1,1)}, // J
        new[] { new Vector2Int(-1,0), new Vector2Int(0,0), new Vector2Int(1,0), new Vector2Int(1,1) }, // L
    };

    // SRS Wall Kicks JLSTZ
    private static readonly Vector2Int[][] KicksJLSTZ =
    {
        new[] { Vector2Int.zero, new Vector2Int(-1, 0), new Vector2Int(-1, 1), new Vector2Int(0,-2), new Vector2Int(-1,-2) },
        new[] { Vector2Int.zero, new Vector2Int( 1, 0), new Vector2Int( 1,-1), new Vector2Int(0, 2), new Vector2Int( 1, 2) },
        new[] { Vector2Int.zero, new Vector2Int( 1, 0), new Vector2Int( 1,-1), new Vector2Int(0, 2), new Vector2Int( 1, 2) },
        new[] { Vector2Int.zero, new Vector2Int(-1, 0), new Vector2Int(-1, 1), new Vector2Int(0,-2), new Vector2Int(-1,-2) },
        new[] { Vector2Int.zero, new Vector2Int( 1, 0), new Vector2Int( 1, 1), new Vector2Int(0,-2), new Vector2Int( 1,-2) },
        new[] { Vector2Int.zero, new Vector2Int(-1, 0), new Vector2Int(-1,-1), new Vector2Int(0, 2), new Vector2Int(-1, 2) },
        new[] { Vector2Int.zero, new Vector2Int(-1, 0), new Vector2Int(-1,-1), new Vector2Int(0, 2), new Vector2Int(-1, 2) },
        new[] { Vector2Int.zero, new Vector2Int( 1, 0), new Vector2Int( 1, 1), new Vector2Int(0,-2), new Vector2Int( 1,-2) },
    };

    // SRS Wall Kicks I
    private static readonly Vector2Int[][] KicksI =
    {
        new[] { Vector2Int.zero, new Vector2Int(-2, 0), new Vector2Int( 1, 0), new Vector2Int(-2,-1), new Vector2Int( 1, 2) },
        new[] { Vector2Int.zero, new Vector2Int( 2, 0), new Vector2Int(-1, 0), new Vector2Int( 2, 1), new Vector2Int(-1,-2) },
        new[] { Vector2Int.zero, new Vector2Int(-1, 0), new Vector2Int( 2, 0), new Vector2Int(-1, 2), new Vector2Int( 2,-1) },
        new[] { Vector2Int.zero, new Vector2Int( 1, 0), new Vector2Int(-2, 0), new Vector2Int( 1,-2), new Vector2Int(-2, 1) },
        new[] { Vector2Int.zero, new Vector2Int( 2, 0), new Vector2Int(-1, 0), new Vector2Int( 2, 1), new Vector2Int(-1,-2) },
        new[] { Vector2Int.zero, new Vector2Int(-2, 0), new Vector2Int( 1, 0), new Vector2Int(-2,-1), new Vector2Int( 1, 2) },
        new[] { Vector2Int.zero, new Vector2Int( 1, 0), new Vector2Int(-2, 0), new Vector2Int( 1,-2), new Vector2Int(-2, 1) },
        new[] { Vector2Int.zero, new Vector2Int(-1, 0), new Vector2Int( 2, 0), new Vector2Int(-1, 2), new Vector2Int( 2,-1) },
    };

    private static readonly int[,] KickLookup = { { 0, 7 }, { 2, 1 }, { 4, 3 }, { 6, 5 } };

    public static Vector2Int[] GetKicks(TetrominoType type, int fromState, bool clockwise)
    {
        int idx = KickLookup[fromState, clockwise ? 0 : 1];
        return type == TetrominoType.I ? KicksI[idx] : KicksJLSTZ[idx];
    }

    public static Vector2Int RotateCW(Vector2Int v)  => new Vector2Int( v.y, -v.x);
    public static Vector2Int RotateCCW(Vector2Int v) => new Vector2Int(-v.y,  v.x);
}
