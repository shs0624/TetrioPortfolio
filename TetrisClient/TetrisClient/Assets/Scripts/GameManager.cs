using UnityEngine;
using UnityEngine.InputSystem;

/// <summary>
/// Main game loop: input (DAS/ARR), gravity, 7-bag spawning, hold piece, game state.
/// [SERVER_HOOK] methods are the server integration entry points.
///
/// Controls:
///   Left / Right      Move (DAS)
///   Up / X            Rotate CW
///   Z / Left Ctrl     Rotate CCW
///   Down              Soft Drop
///   Space             Hard Drop
///   Left/Right Shift  Hold
/// </summary>
public class GameManager : MonoBehaviour
{
    [Header("Scene References")]
    public Board board;
    public Piece piece;

    [Header("Block Visuals (I O T S Z J L)")]
    public Sprite[]  blockSprites;
    public Material  blockMaterial;

    [Header("Hold Display")]
    public Transform holdDisplayRoot;   // world-space anchor (left of board)

    [Header("Input Timing (seconds)")]
    public float dasDelay    = 0.167f;
    public float arrInterval = 0.033f;

    // ── DAS ──────────────────────────────────────────────────────────────────
    private float _dasTimer;
    private float _arrTimer;
    private int   _dasDir;

    // ── 7-bag ─────────────────────────────────────────────────────────────────
    private readonly TetrominoType[] _bag = new TetrominoType[7];
    private int _bagIdx = 7;

    // ── Hold ─────────────────────────────────────────────────────────────────
    private TetrominoType? _heldType  = null;
    private bool           _canHold   = true;
    private readonly Transform[] _holdCells = new Transform[4];

    private bool _gameOver;

    // ── Unity lifecycle ──────────────────────────────────────────────────────

    private void Start()
    {
        board.blockSprites  = blockSprites;
        board.blockMaterial = blockMaterial;
        piece.blockSprites  = blockSprites;
        piece.blockMaterial = blockMaterial;
        SpawnNext();
    }

    private void Update()
    {
        if (_gameOver) return;
        var kb = Keyboard.current;
        if (kb == null) return;

        ProcessInput(kb);

        bool softDrop = kb.downArrowKey.isPressed || kb.sKey.isPressed;
        piece.Tick(Time.deltaTime, softDrop);

        if (piece.IsLocked) SpawnNext();
    }

    // ── Input ────────────────────────────────────────────────────────────────

    private void ProcessInput(Keyboard kb)
    {
        // Rotate
        if (kb.upArrowKey.wasPressedThisFrame || kb.xKey.wasPressedThisFrame)
            piece.RotateCW();
        if (kb.zKey.wasPressedThisFrame || kb.leftCtrlKey.wasPressedThisFrame)
            piece.RotateCCW();

        // Hard drop
        if (kb.spaceKey.wasPressedThisFrame)
            piece.HardDrop();

        // Hold
        if (kb.leftShiftKey.wasPressedThisFrame || kb.rightShiftKey.wasPressedThisFrame)
            TryHold();

        // DAS
        bool leftHeld  = kb.leftArrowKey.isPressed  || kb.aKey.isPressed;
        bool rightHeld = kb.rightArrowKey.isPressed || kb.dKey.isPressed;
        bool leftDown  = kb.leftArrowKey.wasPressedThisFrame  || kb.aKey.wasPressedThisFrame;
        bool rightDown = kb.rightArrowKey.wasPressedThisFrame || kb.dKey.wasPressedThisFrame;

        if (leftDown)  { piece.MoveLeft();  SetDAS(-1); }
        if (rightDown) { piece.MoveRight(); SetDAS( 1); }

        bool oneHeld = leftHeld != rightHeld;
        if (oneHeld)
        {
            int heldDir = leftHeld ? -1 : 1;
            if (heldDir == _dasDir)
            {
                _dasTimer += Time.deltaTime;
                if (_dasTimer >= dasDelay)
                {
                    _arrTimer += Time.deltaTime;
                    while (_arrTimer >= arrInterval)
                    {
                        _arrTimer -= arrInterval;
                        if (heldDir < 0) piece.MoveLeft(); else piece.MoveRight();
                    }
                }
            }
        }
        else if (!leftHeld && !rightHeld)
        {
            _dasDir = 0; _dasTimer = 0f; _arrTimer = 0f;
        }
    }

    private void SetDAS(int dir) { _dasDir = dir; _dasTimer = 0f; _arrTimer = 0f; }

    // ── Hold ─────────────────────────────────────────────────────────────────

    private void TryHold()
    {
        if (!_canHold) return;
        _canHold = false;

        TetrominoType current = piece.Type;
        piece.Cancel();

        if (_heldType == null)
        {
            // 처음 홀드: 현재 블록 저장 후 다음 블록 스폰
            _heldType = current;
            SpawnNext(skipHoldReset: true); // hold 쿨다운은 유지
        }
        else
        {
            // 스왑: 보관된 블록과 현재 블록 교환
            TetrominoType toSpawn = _heldType.Value;
            _heldType = current;
            var spawnPivot = new Vector2Int(4, 18);
            piece.Initialize(board, toSpawn, spawnPivot);
        }

        UpdateHoldDisplay();
    }

    // ── Spawning ─────────────────────────────────────────────────────────────

    private void SpawnNext(bool skipHoldReset = false)
    {
        if (!skipHoldReset) _canHold = true;

        if (_bagIdx >= 7) RefillBag();
        TetrominoType type = _bag[_bagIdx++];

        var spawnPivot = new Vector2Int(4, 18);
        piece.Initialize(board, type, spawnPivot);

        // Top-out check
        foreach (Vector2Int c in TetrominoData.Cells[(int)type])
        {
            if (!board.IsOccupied(spawnPivot + c)) continue;
            _gameOver = true;
            Debug.Log("[GameManager] Game Over");
            // [SERVER_HOOK] NotifyServerGameOver();
            return;
        }

        // 홀드 디스플레이 쿨다운 해제 반영
        if (!skipHoldReset) UpdateHoldDisplay();
    }

    private void RefillBag()
    {
        for (int i = 0; i < 7; i++) _bag[i] = (TetrominoType)i;
        for (int i = 6; i > 0; i--)
        {
            int j = Random.Range(0, i + 1);
            TetrominoType tmp = _bag[i]; _bag[i] = _bag[j]; _bag[j] = tmp;
        }
        _bagIdx = 0;
        // [SERVER_HOOK] Replace with server-provided next piece
    }

    // ── Hold Display ─────────────────────────────────────────────────────────

    private void UpdateHoldDisplay()
    {
        // 기존 홀드 셀 제거
        for (int i = 0; i < _holdCells.Length; i++)
        {
            if (_holdCells[i] != null)
            {
                Destroy(_holdCells[i].gameObject);
                _holdCells[i] = null;
            }
        }

        if (_heldType == null || holdDisplayRoot == null) return;

        Vector2Int[] cells = TetrominoData.Cells[(int)_heldType.Value];
        Sprite sprite      = blockSprites[(int)_heldType.Value];

        // 피스를 홀드 디스플레이 중앙에 맞추기 위한 오프셋 계산
        float avgX = 0f, avgY = 0f;
        for (int i = 0; i < cells.Length; i++) { avgX += cells[i].x; avgY += cells[i].y; }
        avgX /= cells.Length; avgY /= cells.Length;

        Vector3 center = holdDisplayRoot.position;

        for (int i = 0; i < 4; i++)
        {
            var go = new GameObject("HoldCell_" + i);
            go.transform.SetParent(holdDisplayRoot, false);
            go.transform.position = center + new Vector3(cells[i].x - avgX, cells[i].y - avgY, 0f);

            var sr = go.AddComponent<SpriteRenderer>();
            sr.sprite         = sprite;
            sr.sharedMaterial = blockMaterial;
            sr.sortingOrder   = 2;

            // 홀드 쿨다운 중 → 회색으로 표시
            if (!_canHold)
            {
                Color c = new Color(0.45f, 0.45f, 0.45f, 1f);
                sr.color = c;
            }

            _holdCells[i] = go.transform;
        }
    }

    // ── Server hooks ─────────────────────────────────────────────────────────

    public void OnServerBoardUpdate(int[,] boardState) => board.ApplyBoardState(boardState);
    public void OnServerGameOver() { _gameOver = true; }
}
