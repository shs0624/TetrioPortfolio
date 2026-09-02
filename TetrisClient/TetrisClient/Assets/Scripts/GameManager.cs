using UnityEngine;
using UnityEngine.InputSystem;

/// <summary>
/// GameScene의 컨트롤러. 블록 생성/라인 클리어/보드 갱신은 전부 서버 패킷이 주도하며,
/// 이 클래스는 매칭 후 게임 준비(REQ/RES_GAME_READY) → 카운트다운(ACK_COUNTDOWN) 네트워크 훅,
/// 키 입력을 서버로 전송하는 것, 서버가 보내는 상태를 화면에 반영하는 진입점([SERVER_HOOK] 메서드)을 담당한다.
///
/// Controls (모두 서버로 en_INPUT_TYPE만 전송 — 로컬 이동/회전은 하지 않음):
///   Left / Right      en_INPUT_LEFT / en_INPUT_RIGHT
///   Down              en_INPUT_SOFTDROP
///   Space             en_INPUT_HARDDROP
///   Up / X            en_INPUT_ROTATE_CLOCKWISE
///   Z                 en_INPUT_ROTATE_COUNTERCLOCKWISE
///   Left/Right Shift  en_INPUT_HOLD
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

    // ── Hold ─────────────────────────────────────────────────────────────────
    // BOARDUPDATE의 HoldingBlock 필드로 서버가 알려주는 값을 그대로 표시한다(클라이언트는 예측하지 않음).
    private TetBlockType? _heldType = null;
    private readonly Transform[] _holdCells = new Transform[4];

    private bool _gameOver;

    // ── Countdown ────────────────────────────────────────────────────────────
    private CountdownUI _countdownUI;
    private bool        _countdownFinished; // 카운트다운이 끝나기 전에는 입력을 보내지 않는다.

    // ── Opponent board ───────────────────────────────────────────────────────
    // 아직 상대방 보드 렌더링 UI가 없어 데이터만 보관한다. (row-major, row0=서버 상단, 200바이트)
    private byte[] _lastOpponentBoard;

    // ── Unity lifecycle ──────────────────────────────────────────────────────

    private void Start()
    {
        board.blockSprites  = blockSprites;
        board.blockMaterial = blockMaterial;
        piece.blockSprites  = blockSprites;
        piece.blockMaterial = blockMaterial;

        _countdownUI = new GameObject("CountdownUI").AddComponent<CountdownUI>();

        // [SERVER_HOOK] 매칭 후 게임 씬 진입 시 REQ_GAME_READY 전송, RES_GAME_READY / ACK_COUNTDOWN 응답 대기
        if (Client.Instance != null)
        {
            Client.Instance.OnGameReadyResponse += OnGameReadyResponse;
            Client.Instance.OnCountdown         += OnServerCountdown;
            Client.Instance.OnBoardUpdate       += OnServerBoardUpdate;
            Client.Instance.OnBlockUpdate       += OnServerBlockUpdate;
            Client.Instance.RequestGameReady();
        }
        else
        {
            Debug.LogError("[GameManager] Client.Instance가 null입니다. GameScene에 Client 컴포넌트가 있는지 확인해주세요.");
        }
    }

    private void OnDestroy()
    {
        if (Client.Instance != null)
        {
            Client.Instance.OnGameReadyResponse -= OnGameReadyResponse;
            Client.Instance.OnCountdown         -= OnServerCountdown;
            Client.Instance.OnBoardUpdate       -= OnServerBoardUpdate;
            Client.Instance.OnBlockUpdate       -= OnServerBlockUpdate;
        }
    }

    private void Update()
    {
        if (!CanSendInput()) return;

        var kb = Keyboard.current;
        if (kb == null) return;

        if (kb.leftArrowKey.wasPressedThisFrame)
            Client.Instance.SendGameInput(en_INPUT_TYPE.en_INPUT_LEFT);
        if (kb.rightArrowKey.wasPressedThisFrame)
            Client.Instance.SendGameInput(en_INPUT_TYPE.en_INPUT_RIGHT);
        if (kb.downArrowKey.wasPressedThisFrame)
            Client.Instance.SendGameInput(en_INPUT_TYPE.en_INPUT_SOFTDROP);
        if (kb.spaceKey.wasPressedThisFrame)
            Client.Instance.SendGameInput(en_INPUT_TYPE.en_INPUT_HARDDROP);
        if (kb.upArrowKey.wasPressedThisFrame || kb.xKey.wasPressedThisFrame)
            Client.Instance.SendGameInput(en_INPUT_TYPE.en_INPUT_ROTATE_CLOCKWISE);
        if (kb.zKey.wasPressedThisFrame)
            Client.Instance.SendGameInput(en_INPUT_TYPE.en_INPUT_ROTATE_COUNTERCLOCKWISE);
        if (kb.leftShiftKey.wasPressedThisFrame || kb.rightShiftKey.wasPressedThisFrame)
            Client.Instance.SendGameInput(en_INPUT_TYPE.en_INPUT_HOLD);
    }

    /// <summary>카운트다운이 끝났고, 서버가 보내준 낙하 블록(DropBlock)이 있을 때만 입력을 보낸다.</summary>
    private bool CanSendInput()
        => _countdownFinished
        && Client.Instance != null
        && piece != null
        && piece.BlockType != TetBlockType.NoneBlock;

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

        TetBlockType type = _heldType.Value;

        // 스폰 회전 상태(rotate=0) 기준으로 채워진 칸 좌표 수집 (row는 서버 기준 아래로 증가)
        var cells = new System.Collections.Generic.List<Vector2Int>(4);
        for (int row = 0; row < ShapeTable.ShapeSize; row++)
            for (int col = 0; col < ShapeTable.ShapeSize; col++)
                if (ShapeTable.Table[(int)type, 0, row, col] != 0)
                    cells.Add(new Vector2Int(col, row));

        if (cells.Count == 0) return;

        // 피스를 홀드 디스플레이 중앙에 맞추기 위한 오프셋 계산
        float avgX = 0f, avgY = 0f;
        for (int i = 0; i < cells.Count; i++) { avgX += cells[i].x; avgY += cells[i].y; }
        avgX /= cells.Count; avgY /= cells.Count;

        Vector3 center = holdDisplayRoot.position;
        Sprite  sprite = blockSprites[(int)type - 1]; // IBlock(1)..LBlock(7) → 배열 인덱스 0..6

        for (int i = 0; i < cells.Count && i < _holdCells.Length; i++)
        {
            var go = new GameObject("HoldCell_" + i);
            go.transform.SetParent(holdDisplayRoot, false);
            // row는 서버 기준 아래로 증가하므로, 화면에 똑바로 보이도록 y축을 뒤집어 배치
            go.transform.position = center + new Vector3(cells[i].x - avgX, -(cells[i].y - avgY), 0f);

            var sr = go.AddComponent<SpriteRenderer>();
            sr.sprite         = sprite;
            sr.sharedMaterial = blockMaterial;
            sr.sortingOrder   = 2;

            _holdCells[i] = go.transform;
        }
    }

    // ── Server hooks ─────────────────────────────────────────────────────────

    /// <summary>Client.OnBoardUpdate 콜백. 내 보드/홀드 표시를 갱신하고, 상대 보드/다음 블록은 보관만 한다.</summary>
    private void OnServerBoardUpdate(TetBlockType holdingBlock, TetBlockType[] nextBag, byte[] myBoard, byte[] opponentBoard)
    {
        board.ApplyServerBoard(myBoard);
        piece.RefreshGhost(); // 보드가 바뀌었으니 착지 예측도 다시 계산 (BlockUpdate로도 갱신되지만 이중 안전장치)

        _heldType = holdingBlock == TetBlockType.NoneBlock ? (TetBlockType?)null : holdingBlock;
        UpdateHoldDisplay();

        _lastOpponentBoard = opponentBoard; // [SERVER_HOOK] 상대방 보드 UI가 생기면 여기서 그리면 됨
        Debug.Log($"[GameManager] BOARDUPDATE 수신. Hold={holdingBlock} NextBag=[{string.Join(",", nextBag)}]");
    }

    /// <summary>Client.OnBlockUpdate 콜백. 현재 낙하 중인 블록 위치/모양을 갱신한다.</summary>
    private void OnServerBlockUpdate(TetBlockType blockType, byte rotate, sbyte x, sbyte y)
        => piece.ApplyServerState(board, blockType, rotate, x, y);

    public void OnServerGameOver() { _gameOver = true; }

    /// <summary>Client.OnGameReadyResponse 콜백. RES_GAME_READY 도착 확인용.</summary>
    private void OnGameReadyResponse(bool success)
    {
        if (success)
            Debug.Log("[GameManager] RES_GAME_READY 수신: 게임 시작 준비 완료.");
        else
            Debug.LogWarning("[GameManager] RES_GAME_READY 수신: 서버가 게임 준비 요청을 거부했습니다.");
    }

    /// <summary>
    /// Client.OnCountdown 콜백. ACK_COUNTDOWN(seconds) 수신 시 카운트다운 연출을 재생한다.
    /// 연출이 끝나면 서버가 보내는 보드/블록 갱신 패킷을 기다리는 상태가 된다.
    /// </summary>
    private void OnServerCountdown(int seconds)
    {
        Debug.Log($"[GameManager] ACK_COUNTDOWN 수신: {seconds}초 카운트다운 시작.");
        _countdownFinished = false;
        _countdownUI.PlayCountdown(seconds, () =>
        {
            _countdownFinished = true;
            Debug.Log("[GameManager] 카운트다운 종료. 입력 전송 가능 상태로 전환.");
        });
    }
}
