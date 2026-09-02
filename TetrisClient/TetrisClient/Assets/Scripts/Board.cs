using UnityEngine;

/// <summary>
/// Manages the Tetris grid: locked cell rendering and grid overlay. Board state is entirely
/// server-driven — call ApplyBoardState() when a board snapshot arrives from the server.
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

    // ── Server sync ──────────────────────────────────────────────────────────

    /// <summary>
    /// 서버 BoardUpdate의 보드 바이트(row-major, row*Width+col, row0=서버 상단)를 그대로 반영한다.
    /// 서버는 row 증가 = 아래쪽, 클라이언트 그리드는 row 증가 = 위쪽이라 세로로 뒤집어 배치한다.
    /// </summary>
    public void ApplyServerBoard(byte[] boardBytes)
    {
        for (int row = 0; row < Height; row++)
            for (int col = 0; col < Width; col++)
            {
                var type = (TetBlockType)boardBytes[row * Width + col];
                int gx = col;
                int gy = Height - 1 - row;

                if (_grid[gx, gy] != null) Destroy(_grid[gx, gy].gameObject);
                _grid[gx, gy] = type != TetBlockType.NoneBlock
                    ? CreateCell(type, new Vector2Int(gx, gy))
                    : null;
            }
    }

    /// <summary>
    /// 서버 row 기준(0=상단, Height=바닥 아래) 좌표로 점유 여부를 확인한다. 고스트 피스 착지 계산에 사용.
    /// 벽 밖/바닥 아래는 점유된 것으로, 보드 위쪽은 비어있는 것으로 취급한다(서버 CollisionCheck와 동일한 규칙).
    /// </summary>
    public bool IsCellOccupied(int col, int serverRow)
    {
        if (col < 0 || col >= Width) return true;
        if (serverRow >= Height) return true;
        if (serverRow < 0) return false;

        int clientRow = Height - 1 - serverRow;
        return _grid[col, clientRow] != null;
    }

    // ── Internal ─────────────────────────────────────────────────────────────

    private Transform CreateCell(TetBlockType type, Vector2Int pos)
    {
        var go = new GameObject(string.Format("cell_{0}_{1}", pos.x, pos.y));
        go.transform.SetParent(cellContainer, false);
        go.transform.position = GridToWorld(pos);
        var sr = go.AddComponent<SpriteRenderer>();
        sr.sharedMaterial = blockMaterial;

        if (type == TetBlockType.GarbageBlock)
        {
            // 가비지 전용 스프라이트가 없어 임시로 기존 스프라이트를 회색 틴트로 대체한다.
            sr.sprite = blockSprites[0];
            sr.color  = new Color(0.4f, 0.4f, 0.4f, 1f);
        }
        else
        {
            sr.sprite = blockSprites[(int)type - 1]; // IBlock(1)..LBlock(7) → 배열 인덱스 0..6
        }

        return go.transform;
    }
}
