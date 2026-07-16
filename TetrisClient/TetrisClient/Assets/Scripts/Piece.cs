using UnityEngine;

/// <summary>
/// The active falling tetromino.
/// Handles grid movement, SRS rotation, gravity, lock delay, and ghost preview.
/// </summary>
public class Piece : MonoBehaviour
{
    // ── Public state ─────────────────────────────────────────────────────────
    public Board         Board         { get; private set; }
    public TetrominoType Type          { get; private set; }
    public Vector2Int    Pivot         { get; private set; }
    public Vector2Int[]  Cells         { get; private set; }
    public int           RotationState { get; private set; }
    public bool          IsLocked      { get; private set; }
    public bool          IsCancelled   { get; private set; }

    [Header("Timing (seconds)")]
    public float lockDelay       = 0.5f;
    public float gravityInterval = 1.0f;
    public float softDropFactor  = 20f;

    [HideInInspector] public Sprite[]  blockSprites;
    [HideInInspector] public Material  blockMaterial;

    private readonly Transform[] _activeCells = new Transform[4];
    private readonly Transform[] _ghostCells  = new Transform[4];

    private float _gravTimer;
    private float _lockTimer;
    private bool  _lockActive;

    // ── Initialization ───────────────────────────────────────────────────────

    public void Initialize(Board board, TetrominoType type, Vector2Int pivot)
    {
        Board         = board;
        Type          = type;
        Pivot         = pivot;
        Cells         = (Vector2Int[])TetrominoData.Cells[(int)type].Clone();
        RotationState = 0;
        IsLocked      = false;
        IsCancelled   = false;
        _gravTimer    = 0f;
        _lockTimer    = 0f;
        _lockActive   = false;

        DestroyVisuals();
        BuildVisuals();
        RefreshPositions();
    }

    /// <summary>Removes the piece from play without locking it. Used for hold swaps.</summary>
    public void Cancel()
    {
        IsCancelled = true;
        DestroyVisuals();
    }

    // ── Public actions ───────────────────────────────────────────────────────

    public bool MoveLeft()  => TryMove(Vector2Int.left);
    public bool MoveRight() => TryMove(Vector2Int.right);
    public bool RotateCW()  => TryRotate(true);
    public bool RotateCCW() => TryRotate(false);

    public void SoftDropStep()
    {
        if (!TryMove(Vector2Int.down)) ActivateLock();
        else _gravTimer = 0f;
    }

    public void HardDrop()
    {
        while (TryMove(Vector2Int.down)) { }
        Lock();
    }

    // ── Tick ─────────────────────────────────────────────────────────────────

    public void Tick(float dt, bool softDropping)
    {
        if (IsLocked || IsCancelled) return;

        float interval = gravityInterval / (softDropping ? softDropFactor : 1f);
        _gravTimer += dt;
        if (_gravTimer >= interval)
        {
            _gravTimer = 0f;
            if (!TryMove(Vector2Int.down)) ActivateLock();
        }

        if (_lockActive)
        {
            _lockTimer += dt;
            if (_lockTimer >= lockDelay) Lock();
        }
    }

    // ── Internal movement ────────────────────────────────────────────────────

    private bool TryMove(Vector2Int delta)
    {
        Vector2Int next = Pivot + delta;
        if (!IsValidPlacement(next, Cells)) return false;
        Pivot = next;
        ResetLock();
        RefreshPositions();
        return true;
    }

    private bool TryRotate(bool cw)
    {
        Vector2Int[] rotated = new Vector2Int[4];
        for (int i = 0; i < 4; i++)
            rotated[i] = cw ? TetrominoData.RotateCW(Cells[i]) : TetrominoData.RotateCCW(Cells[i]);

        int newState = (RotationState + (cw ? 1 : 3)) % 4;
        foreach (Vector2Int kick in TetrominoData.GetKicks(Type, RotationState, cw))
        {
            Vector2Int testPivot = Pivot + kick;
            if (!IsValidPlacement(testPivot, rotated)) continue;
            Pivot         = testPivot;
            Cells         = rotated;
            RotationState = newState;
            ResetLock();
            RefreshPositions();
            return true;
        }
        return false;
    }

    private bool IsValidPlacement(Vector2Int pivot, Vector2Int[] cells)
    {
        foreach (Vector2Int c in cells)
            if (Board.IsOccupied(pivot + c)) return false;
        return true;
    }

    private void Lock()
    {
        if (IsLocked) return;
        IsLocked = true;
        Board.LockPiece(Type, Pivot, Cells);
        DestroyVisuals();
    }

    private void ActivateLock() { if (!_lockActive) { _lockActive = true; _lockTimer = 0f; } }
    private void ResetLock()    { _lockActive = false; _lockTimer = 0f; }

    // ── Visuals ──────────────────────────────────────────────────────────────

    private void BuildVisuals()
    {
        for (int i = 0; i < 4; i++)
        {
            _activeCells[i] = MakeCell("Active_" + i, 1, 1.0f);
            _ghostCells[i]  = MakeCell("Ghost_"  + i, 0, 0.3f);
        }
    }

    private Transform MakeCell(string goName, int sortOrder, float alpha)
    {
        var go = new GameObject(goName);
        go.transform.SetParent(transform, false);
        var sr = go.AddComponent<SpriteRenderer>();
        sr.sprite         = blockSprites[(int)Type];
        sr.sharedMaterial = blockMaterial;
        sr.sortingOrder   = sortOrder;
        Color c = sr.color; c.a = alpha; sr.color = c;
        return go.transform;
    }

    private void RefreshPositions()
    {
        for (int i = 0; i < 4; i++)
            if (_activeCells[i] != null)
                _activeCells[i].position = Board.GridToWorld(Pivot + Cells[i]);

        Vector2Int ghost = GhostPivot();
        for (int i = 0; i < 4; i++)
            if (_ghostCells[i] != null)
                _ghostCells[i].position = Board.GridToWorld(ghost + Cells[i]);
    }

    private Vector2Int GhostPivot()
    {
        Vector2Int p = Pivot;
        while (IsValidPlacement(p + Vector2Int.down, Cells)) p += Vector2Int.down;
        return p;
    }

    private void DestroyVisuals()
    {
        for (int i = 0; i < 4; i++)
        {
            if (_activeCells[i] != null) { Destroy(_activeCells[i].gameObject); _activeCells[i] = null; }
            if (_ghostCells[i]  != null) { Destroy(_ghostCells[i].gameObject);  _ghostCells[i]  = null; }
        }
    }
}
