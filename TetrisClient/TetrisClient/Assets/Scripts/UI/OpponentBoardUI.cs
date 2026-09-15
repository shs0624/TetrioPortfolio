using UnityEngine;

/// <summary>
/// Board.ApplyServerBoard/Piece.PlaceCells와 동일한 좌표 변환 공식을 축소 버전으로 재구현
///
/// 이 컴포넌트가 붙는 GameObject(배경/테두리 자식 포함)는 GameScene에 미리 배치된
/// 씬 오브젝트다(런타임에 코드로 생성하지 않음) — 위치를 바꾸고 싶으면 씬에서 이
/// 오브젝트를 옮기면 되고, 그 위치가 곧 미니 보드의 좌하단 코너(원점)가 된다.
/// </summary>
public class OpponentBoardUI : MonoBehaviour
{
    const int Width  = TetrominoData.BoardWidth;  // 10
    const int Height = TetrominoData.BoardHeight; // 20

    [Tooltip("미니 보드 한 칸의 월드 크기(유닛). 배경/테두리 자식 오브젝트 크기와 맞춰서 조정할 것.")]
    [SerializeField] float cellScale = 0.25f;

    Sprite[]  _blockSprites;
    Material  _blockMaterial;

    readonly Transform[,] _lockedCells = new Transform[Width, Height];
    readonly Transform[]  _pieceCells  = new Transform[4];

    /// <summary>스프라이트/머티리얼 참조를 넘겨 초기화한다. GameManager.Start()에서 한 번 호출.</summary>
    public void Init(Sprite[] blockSprites, Material blockMaterial)
    {
        _blockSprites  = blockSprites;
        _blockMaterial = blockMaterial;
    }

    /// <summary>
    /// 서버 BoardUpdate의 OpponentBoard(row-major, row*Width+col, row0=서버 상단)를 그대로 반영한다.
    /// Board.ApplyServerBoard와 동일하게 서버 row를 세로로 뒤집어 배치한다.
    /// </summary>
    public void ApplyBoard(byte[] boardBytes)
    {
        for (int row = 0; row < Height; row++)
            for (int col = 0; col < Width; col++)
            {
                var type = (TetBlockType)boardBytes[row * Width + col];
                int gx = col;
                int gy = Height - 1 - row;

                if (_lockedCells[gx, gy] != null) Destroy(_lockedCells[gx, gy].gameObject);
                _lockedCells[gx, gy] = type != TetBlockType.NoneBlock
                    ? MakeCell(type, gx, gy, sortOrder: 1, alpha: 1f)
                    : null;
            }
    }

    /// <summary>
    /// 서버 BlockUpdate(IsSelf=false)의 상대 낙하 블록을 그대로 반영한다.
    /// blockType이 NoneBlock이면 화면에서 지운다.
    /// </summary>
    public void ApplyPiece(TetBlockType blockType, byte rotate, sbyte x, sbyte y)
    {
        DestroyCells(_pieceCells);
        if (blockType == TetBlockType.NoneBlock) return;

        int cellIdx = 0;
        for (int row = 0; row < ShapeTable.ShapeSize; row++)
        {
            for (int col = 0; col < ShapeTable.ShapeSize; col++)
            {
                if (ShapeTable.Table[(int)blockType, rotate, row, col] == 0) continue;
                if (cellIdx >= _pieceCells.Length) continue; // 방어: 정상 블록은 항상 4칸

                int gx = x + col;
                int gy = Height - 1 - (y + row);
                _pieceCells[cellIdx++] = MakeCell(blockType, gx, gy, sortOrder: 2, alpha: 1f);
            }
        }
    }

    // ── Internal ─────────────────────────────────────────────────────────────

    /// <summary>이 오브젝트의 씬 배치 위치를 미니 보드의 좌하단 코너(원점)로 삼는다.</summary>
    Vector3 MiniGridToWorld(int gx, int gy)
        => transform.position + new Vector3((gx + 0.5f) * cellScale, (gy + 0.5f) * cellScale, 0f);

    Transform MakeCell(TetBlockType type, int gx, int gy, int sortOrder, float alpha)
    {
        var go = new GameObject($"cell_{gx}_{gy}");
        go.transform.SetParent(transform, false);
        go.transform.position   = MiniGridToWorld(gx, gy);
        go.transform.localScale = Vector3.one * cellScale;

        var sr = go.AddComponent<SpriteRenderer>();
        sr.sharedMaterial = _blockMaterial;
        sr.sortingOrder   = sortOrder;

        if (type == TetBlockType.GarbageBlock)
        {
            sr.sprite = _blockSprites[0];
            sr.color  = new Color(0.4f, 0.4f, 0.4f, alpha);
        }
        else
        {
            sr.sprite = _blockSprites[(int)type - 1]; // IBlock(1)..LBlock(7) → 배열 인덱스 0..6
            sr.color  = new Color(1f, 1f, 1f, alpha);
        }

        return go.transform;
    }

    void DestroyCells(Transform[] cells)
    {
        for (int i = 0; i < cells.Length; i++)
        {
            if (cells[i] != null) { Destroy(cells[i].gameObject); cells[i] = null; }
        }
    }
}
