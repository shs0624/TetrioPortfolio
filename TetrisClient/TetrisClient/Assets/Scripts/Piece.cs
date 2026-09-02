using UnityEngine;

/// <summary>
/// The active falling tetromino — a rendering shell only. BlockType/Rotate/X/Y are supplied
/// verbatim by the server's BlockUpdate packet; this class draws the matching ShapeTable
/// cells at those board coordinates, plus a client-computed ghost preview of where a hard
/// drop would land (mirrors the server's GetHardDropY algorithm so it always matches).
/// </summary>
public class Piece : MonoBehaviour
{
    // ── Public state ─────────────────────────────────────────────────────────
    public Board        Board       { get; private set; }
    public TetBlockType BlockType   { get; private set; } = TetBlockType.NoneBlock;
    public byte          Rotate     { get; private set; }
    public sbyte         X          { get; private set; }
    public sbyte         Y          { get; private set; }
    public bool          IsCancelled { get; private set; }

    [HideInInspector] public Sprite[]  blockSprites;
    [HideInInspector] public Material  blockMaterial;

    const float GHOST_ALPHA = 0.3f;

    private readonly Transform[] _activeCells = new Transform[4];
    private readonly Transform[] _ghostCells  = new Transform[4];

    /// <summary>
    /// 서버 BlockUpdate(blockType, rotate, x, y)를 그대로 반영해 시각 요소를 다시 그린다.
    /// blockType이 NoneBlock이면 화면에서 지운다.
    /// </summary>
    public void ApplyServerState(Board board, TetBlockType blockType, byte rotate, sbyte x, sbyte y)
    {
        Board       = board;
        BlockType   = blockType;
        Rotate      = rotate;
        X           = x;
        Y           = y;
        IsCancelled = false;

        DestroyCells(_activeCells);
        if (blockType == TetBlockType.NoneBlock)
        {
            DestroyCells(_ghostCells);
            return;
        }

        PlaceCells(_activeCells, blockType, rotate, x, y, sortOrder: 1, alpha: 1f);
        RefreshGhost();
    }

    /// <summary>
    /// 현재 블록이 하드 드랍하면 착지할 위치를 서버 GetHardDropY와 동일한 알고리즘으로 계산해
    /// 반투명 고스트로 다시 그린다. 보드 상태만 바뀌었을 때(다음 블록 스폰 직후 등)도 호출할 수 있다.
    /// </summary>
    public void RefreshGhost()
    {
        DestroyCells(_ghostCells);
        if (BlockType == TetBlockType.NoneBlock || Board == null) return;

        int ghostY = ComputeGhostServerY();
        if (ghostY == Y) return; // 이미 바닥 — 활성 피스와 겹치므로 고스트 생략

        PlaceCells(_ghostCells, BlockType, Rotate, X, ghostY, sortOrder: 0, alpha: GHOST_ALPHA);
    }

    /// <summary>서버 TetrisServer::GetHardDropY와 1:1 대응하는 착지 Y 계산.</summary>
    private int ComputeGhostServerY()
    {
        int retY = Board.Height - 1;

        for (int col = 0; col < ShapeTable.ShapeSize; col++)
        {
            int blockY = -1;
            for (int row = ShapeTable.ShapeSize - 1; row >= 0; row--)
            {
                if (ShapeTable.Table[(int)BlockType, Rotate, row, col] != 0)
                {
                    blockY = row;
                    break;
                }
            }
            if (blockY == -1) continue;

            int nx = X + col;
            int ny = Y + blockY;

            int landY = Board.Height - 1;
            for (int i = ny + 1; i < Board.Height; i++)
            {
                if (Board.IsCellOccupied(nx, i))
                {
                    landY = i - 1;
                    break;
                }
            }

            landY -= blockY;
            retY = Mathf.Min(landY, retY);
        }

        return retY;
    }

    /// <summary>블록을 화면에서 지운다 (홀드 스왑 등). 대상 없음 상태와 동일하게 처리한다.</summary>
    public void Cancel()
    {
        IsCancelled = true;
        DestroyCells(_activeCells);
        DestroyCells(_ghostCells);
    }

    // ── Visuals ──────────────────────────────────────────────────────────────

    private void PlaceCells(Transform[] cells, TetBlockType blockType, byte rotate, int originX, int originY, int sortOrder, float alpha)
    {
        int cellIdx = 0;
        for (int row = 0; row < ShapeTable.ShapeSize; row++)
        {
            for (int col = 0; col < ShapeTable.ShapeSize; col++)
            {
                if (ShapeTable.Table[(int)blockType, rotate, row, col] == 0) continue;
                if (cellIdx >= cells.Length) continue; // 방어: 정상 블록은 항상 4칸

                int boardCol       = originX + col;
                int boardRowServer = originY + row;
                int clientRow      = Board.Height - 1 - boardRowServer;

                var cell = MakeCell(blockType, sortOrder, alpha);
                cell.position = Board.GridToWorld(boardCol, clientRow);
                cells[cellIdx++] = cell;
            }
        }
    }

    private Transform MakeCell(TetBlockType blockType, int sortOrder, float alpha)
    {
        var go = new GameObject("Cell");
        go.transform.SetParent(transform, false);
        var sr = go.AddComponent<SpriteRenderer>();
        sr.sharedMaterial = blockMaterial;
        sr.sortingOrder   = sortOrder;

        if (blockType == TetBlockType.GarbageBlock)
        {
            sr.sprite = blockSprites[0];
            sr.color  = new Color(0.4f, 0.4f, 0.4f, alpha);
        }
        else
        {
            sr.sprite = blockSprites[(int)blockType - 1];
            sr.color  = new Color(1f, 1f, 1f, alpha);
        }

        return go.transform;
    }

    private void DestroyCells(Transform[] cells)
    {
        for (int i = 0; i < cells.Length; i++)
        {
            if (cells[i] != null) { Destroy(cells[i].gameObject); cells[i] = null; }
        }
    }
}
