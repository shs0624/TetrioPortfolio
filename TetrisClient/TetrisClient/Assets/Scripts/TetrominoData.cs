using UnityEngine;

public enum TetrominoType { I = 0, O = 1, T = 2, S = 3, Z = 4, J = 5, L = 6 }

/// <summary>Pure data: board constants, cell shapes.</summary>
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
}
