using UnityEngine;

/// <summary>
/// Manages the Tetris grid: locked cell state, rendering, line-clear logic, and grid overlay.
/// [SERVER_HOOK] Call ApplyBoardState() when a board snapshot arrives from the server.
/// </summary>
public class Board : MonoBehaviour
{
    public const int Width  = TetrominoData.BoardWidth;
    public const int Height = TetrominoData.BoardHeight;

    [Header("Cell Rendering")]
    public Transform cellContainer;
    [HideInInspector] public Sprite[]  blockSprites;
    [HideInInspector] public Material  blockMaterial;

    [Header("Grid Overlay")]
    public Transform gridContainer;
    public Sprite    gridCellSprite;

    private Transform[,] _grid;
    private static readonly UnityEngine.Vector3 BoardOrigin =
        new UnityEngine.Vector3(-Width * 0.5f, -Height * 0.5f, 0f);

    void Awake()
    {
        _grid = new Transform[Width, Height];
    }

    void Start()
    {
        if (gridCellSprite != null && gridContainer != null)
            BuildGrid();
    }

    // ── Grid overlay ─────────────────────────────────────────────────────────

    public void BuildGrid()
    {
        for (int y = 0; y < Height; y++)
            for (int x = 0; x < Width; x++)
            {
                var go = new GameObject(string.Format("Grid_{0}_{1}", x, y));
                go.transform.SetParent(gridContainer, false);
                go.transform.position = GridToWorld(x, y);
                var sr = go.AddComponent<SpriteRenderer>();
                sr.sprite       = gridCellSprite;
                sr.sortingOrder = -1;
            }
    }

    // ── Coordinate helpers ───────────────────────────────────────────────────

    public Vector3 GridToWorld(Vector2Int pos)   => GridToWorld(pos.x, pos.y);
    public Vector3 GridToWorld(int col, int row) =>
        BoardOrigin + new Vector3(col + 0.5f, row + 0.5f, 0f);

    public bool IsOccupied(Vector2Int pos)
    {
        if (pos.x < 0 || pos.x >= Width || pos.y < 0) return true;
        if (pos.y >= Height) return false;
        return _grid[pos.x, pos.y] != null;
    }

    // ── Piece locking ────────────────────────────────────────────────────────

    public int LockPiece(TetrominoType type, Vector2Int pivot, Vector2Int[] cells)
    {
        foreach (Vector2Int offset in cells)
        {
            Vector2Int pos = pivot + offset;
            if (pos.x < 0 || pos.x >= Width || pos.y < 0 || pos.y >= Height) continue;
            _grid[pos.x, pos.y] = CreateCell(type, pos);
        }
        return ClearLines();
    }

    // ── Line clear ───────────────────────────────────────────────────────────

    private int ClearLines()
    {
        int count = 0;
        for (int y = 0; y < Height; y++)
        {
            if (!IsRowFull(y)) continue;
            ClearRow(y);
            DropRowsAbove(y);
            y--; count++;
        }
        return count;
    }

    private bool IsRowFull(int y)
    {
        for (int x = 0; x < Width; x++)
            if (_grid[x, y] == null) return false;
        return true;
    }

    private void ClearRow(int y)
    {
        for (int x = 0; x < Width; x++)
        {
            if (_grid[x, y] != null) Destroy(_grid[x, y].gameObject);
            _grid[x, y] = null;
        }
    }

    private void DropRowsAbove(int fromY)
    {
        for (int y = fromY + 1; y < Height; y++)
            for (int x = 0; x < Width; x++)
            {
                _grid[x, y - 1] = _grid[x, y];
                _grid[x, y]     = null;
                if (_grid[x, y - 1] != null)
                    _grid[x, y - 1].position = GridToWorld(x, y - 1);
            }
    }

    // ── Server sync ──────────────────────────────────────────────────────────

    /// <summary>[SERVER_HOOK] boardState[x,y] = piece type index, -1 = empty.</summary>
    public void ApplyBoardState(int[,] boardState)
    {
        for (int x = 0; x < Width; x++)
            for (int y = 0; y < Height; y++)
            {
                if (_grid[x, y] != null) Destroy(_grid[x, y].gameObject);
                _grid[x, y] = boardState[x, y] >= 0
                    ? CreateCell((TetrominoType)boardState[x, y], new Vector2Int(x, y))
                    : null;
            }
    }

    // ── Internal ─────────────────────────────────────────────────────────────

    private Transform CreateCell(TetrominoType type, Vector2Int pos)
    {
        var go = new GameObject(string.Format("cell_{0}_{1}", pos.x, pos.y));
        go.transform.SetParent(cellContainer, false);
        go.transform.position = GridToWorld(pos);
        var sr = go.AddComponent<SpriteRenderer>();
        sr.sprite         = blockSprites[(int)type];
        sr.sharedMaterial = blockMaterial;
        return go.transform;
    }
}
