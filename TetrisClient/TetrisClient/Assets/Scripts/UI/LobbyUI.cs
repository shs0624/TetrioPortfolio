using UnityEngine;
using UnityEngine.UI;
using UnityEngine.SceneManagement;
using TMPro;
using System.Collections;
using System.Collections.Generic;

/// <summary>
/// Lobby Scene : Chat + User List + Matching.
/// [SERVER_HOOK] marks every point that needs network integration.
/// </summary>
public class LobbyUI : MonoBehaviour
{
    // ── Chat ─────────────────────────────────────────────────────────────
    ScrollRect      _chatScroll;
    Transform       _chatContent;
    TMP_InputField  _chatInput;
    Button          _sendBtn;

    // ── User list ─────────────────────────────────────────────────────────
    Transform       _userContent;
    readonly Dictionary<string, GameObject> _users = new Dictionary<string, GameObject>();

    // ── Match button ─────────────────────────────────────────────────────
    Button          _matchBtn;
    Image           _matchBtnImg;
    TextMeshProUGUI _matchBtnTxt;
    bool            _isMatching;
    bool            _isCancelling; // REQ_MATCHING_CANCEL 응답 대기 중 중복 클릭 방지
    Coroutine       _dotAnim;
    Coroutine       _matchTimeoutCoroutine;
    const float     MATCH_TIMEOUT_SEC = 10f; // RES_MATCHING 자체(대기열 진입 여부)를 기다리는 타임아웃

    // ── Confirm dialog (매칭 실패/타임아웃 시 사용) ─────────────────────────
    ConfirmDialogUI _confirmDialog;

    // ── Font (loaded from TMP Resources) ─────────────────────────────────
    TMP_FontAsset   _font;

    // ── Colors ────────────────────────────────────────────────────────────
    static readonly Color C_BTN_NORMAL  = new Color(0.20f, 0.32f, 0.88f, 1f);
    static readonly Color C_BTN_MATCH   = new Color(0.72f, 0.46f, 0.08f, 1f);
    static readonly Color C_AUTHOR      = new Color(0.55f, 0.76f, 1.00f, 1f);
    static readonly Color C_MSG         = new Color(0.88f, 0.90f, 0.96f, 1f);
    static readonly Color C_SYSTEM      = new Color(0.60f, 0.65f, 0.50f, 1f);
    static readonly Color C_LEVEL       = new Color(0.40f, 0.65f, 1.00f, 1f);

    // ════════════════════════════════════════════════════════════════════
    void Start()
    {
        _font = Resources.Load<TMP_FontAsset>("Fonts & Materials/LiberationSans SDF");

        CacheRefs();

        _confirmDialog = new GameObject("ConfirmDialog").AddComponent<ConfirmDialogUI>();

        _sendBtn?.onClick.AddListener(OnSendChat);
        _matchBtn?.onClick.AddListener(OnMatchBtnClick);

        if (_chatInput != null)
            _chatInput.onSubmit.AddListener(_ => OnSendChat());

        if (Client.Instance != null)
        {
            // [레이스 방지] 서버가 로그인 직후 자기 자신의 ACK_CHAT_ENTER를 곳바로 보낼 수 있고,
            // 그 경우 이 이벤트가 씨 로딩 중(LobbyUI.Start 이전)에 이미 도착해 유실될 수 있다.
            // Client가 유지하는 현재 채팅방 스냅샷(ChatRoster)을 먼저 읽어 초기 목록을 채운다.
            foreach (var kv in Client.Instance.ChatRoster)
                AddUser(kv.Key, kv.Value);

            Client.Instance.OnChatMessage   += HandleChatMessage;
            Client.Instance.OnChatUserEnter += HandleChatUserEnter;
            Client.Instance.OnChatUserExit  += HandleChatUserExit;
            Client.Instance.OnMatchResponse += OnMatchResponse;
            Client.Instance.OnMatchCancelResponse += OnMatchCancelResponse;
            Client.Instance.OnMatchSuccess  += HandleMatchSuccess;
        }
        else
        {
            Debug.LogError("[LobbyUI] Client.Instance가 null입니다. LobbyScene에 Client 컴포넌트가 있는지 확인해주세요.");
        }

        AddSystemMessage("Welcome to the lobby!");
    }

    void OnDestroy()
    {
        if (Client.Instance == null) return;
        Client.Instance.OnChatMessage   -= HandleChatMessage;
        Client.Instance.OnChatUserEnter -= HandleChatUserEnter;
        Client.Instance.OnChatUserExit  -= HandleChatUserExit;
        Client.Instance.OnMatchResponse -= OnMatchResponse;
        Client.Instance.OnMatchCancelResponse -= OnMatchCancelResponse;
        Client.Instance.OnMatchSuccess  -= HandleMatchSuccess;
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Chat
    // ════════════════════════════════════════════════════════════════════

    void OnSendChat()
    {
        if (_chatInput == null || string.IsNullOrWhiteSpace(_chatInput.text)) return;
        string msg = _chatInput.text.Trim();
        _chatInput.text = "";
        _chatInput.ActivateInputField();

        Client.Instance?.SendChatMessage(msg);
    }

    /// <summary>Client.OnChatMessage 콜백 (accountNum, nickname, message) — "닉네임 - 메시지" 형태로 출력.</summary>
    void HandleChatMessage(long accountNum, string nickname, string message)
        => AppendChat(nickname, message, C_AUTHOR, C_MSG);

    /// <summary>System notice (white/green tint).</summary>
    public void AddSystemMessage(string content)
        => AppendChat("System", content, C_SYSTEM, C_SYSTEM);

    void AppendChat(string author, string content, Color authorCol, Color textCol)
    {
        if (_chatContent == null) return;

        var go  = new GameObject("ChatMsg");
        go.transform.SetParent(_chatContent, false);

        // Fills parent width; height driven by text
        var le  = go.AddComponent<LayoutElement>();
        le.flexibleWidth = 1;

        var csf = go.AddComponent<ContentSizeFitter>();
        csf.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

        var tmp = go.AddComponent<TextMeshProUGUI>();
        // 사용자 입력(닉네임/메시지)은 <noparse>로 감싸서 리치텍스트 태그 주입을 방지한다.
        tmp.text = string.Format(
            "<color=#{0}><b><noparse>{1}</noparse></b></color> - <color=#{2}><noparse>{3}</noparse></color>",
            ToHex(authorCol), author, ToHex(textCol), content);
        tmp.fontSize           = 30f;
        tmp.font               = _font;
        tmp.enableWordWrapping = true;
        tmp.raycastTarget      = false;

        StartCoroutine(ScrollToBottom());
    }

    IEnumerator ScrollToBottom()
    {
        yield return new WaitForEndOfFrame();
        if (_chatScroll != null) _chatScroll.verticalNormalizedPosition = 0f;
    }

    // ════════════════════════════════════════════════════════════════════
    // ── User list
    // ════════════════════════════════════════════════════════════════════

    /// <summary>Client.OnChatUserEnter 콜백. 닉네임만 유저 목록에 추가하고, 채팅 메시지는 따로 출력하지 않는다.</summary>
    void HandleChatUserEnter(long accountNum, string nickname) => AddUser(accountNum, nickname);

    /// <summary>Client.OnChatUserExit 콜백. 유저 목록에서만 제거한다.</summary>
    void HandleChatUserExit(long accountNum, string nickname) => RemoveUser(nickname);

    /// <summary>A player entered the lobby. 본인(accountNum == Client.MyAccountNum)은 항상 목록 맨 위에 고정한다.</summary>
    public void AddUser(long accountNum, string nickname)
    {
        if (_userContent == null || _users.ContainsKey(nickname)) return;

        bool isSelf = Client.Instance != null && accountNum == Client.Instance.MyAccountNum;

        var go  = new GameObject("User_" + nickname);
        go.transform.SetParent(_userContent, false);

        var le  = go.AddComponent<LayoutElement>();
        le.preferredHeight = 34f;
        le.flexibleWidth   = 1f;

        var tmp = go.AddComponent<TextMeshProUGUI>();
        // 닉네임도 사용자 입력이므로 <noparse>로 감싸 리치텍스트 태그 주입을 방지한다.
        tmp.text = isSelf
            ? string.Format("<color=#{0}><b><noparse>{1}</noparse> (나)</b></color>", ToHex(C_LEVEL), nickname)
            : string.Format("<noparse>{0}</noparse>", nickname);
        tmp.fontSize      = 25f;
        tmp.color         = C_MSG;
        tmp.font          = _font;
        tmp.alignment     = TextAlignmentOptions.Left;
        tmp.raycastTarget = false;

        _users[nickname] = go;

        // 자기 자신은 언제 입장 알림을 받든 항상 맨 위로 고정한다.
        if (isSelf) go.transform.SetAsFirstSibling();
    }

    /// <summary>A player left the lobby. 목록에서만 제거하고 별도 채팅 메시지는 남기지 않는다.</summary>
    public void RemoveUser(string nickname)
    {
        if (!_users.TryGetValue(nickname, out var go)) return;
        Destroy(go);
        _users.Remove(nickname);
    }

    /// <summary>[SERVER_HOOK] Full user-list snapshot from server (서버에 이 스냅샷 패킷이 아직 없음 — 필요 시 연동).</summary>
    public void SetUserList(long[] accountNums, string[] nicknames)
    {
        foreach (var e in _users.Values) Destroy(e);
        _users.Clear();
        for (int i = 0; i < nicknames.Length; i++)
            AddUser(i < accountNums.Length ? accountNums[i] : 0, nicknames[i]);
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Matching
    // ════════════════════════════════════════════════════════════════════

    void OnMatchBtnClick()
    {
        if (_isMatching)
            RequestMatchCancel();  // 매칭 중일 때 다시 누르면 취소 요청
        else
            RequestMatch();
    }

    public void RequestMatch()
    {
        if (_isMatching) return;
        if (Client.Instance == null)
        {
            Debug.LogError("[LobbyUI] Client.Instance가 null입니다.");
            return;
        }

        _isMatching = true;
        ApplyMatchState(true);

        Debug.Log("[LobbyUI] Match REQ sent.");
        Client.Instance.SendMatchRequest();

        if (_matchTimeoutCoroutine != null) StopCoroutine(_matchTimeoutCoroutine);
        _matchTimeoutCoroutine = StartCoroutine(MatchTimeoutCoroutine());
    }

    IEnumerator MatchTimeoutCoroutine()
    {
        yield return new WaitForSeconds(MATCH_TIMEOUT_SEC);
        _matchTimeoutCoroutine = null;

        _isMatching = false;
        ApplyMatchState(false);
        HandleMatchFailure("매칭 응답이 시간 내에 도착하지 않았습니다.");
    }

    /// <summary>
    /// Client.OnMatchResponse 콜백.
    ///   success=true  → 매칭 대기열 진입 성공, "matching…" 상태 유지 (상대방 매칭 완료는 별도 통지, 범위 밖).
    ///   success=false → 실패 처리 후 확인 모달 표시.
    /// </summary>
    public void OnMatchResponse(bool success)
    {
        if (_matchTimeoutCoroutine != null)
        {
            StopCoroutine(_matchTimeoutCoroutine);
            _matchTimeoutCoroutine = null;
        }

        if (!success)
        {
            _isMatching = false;
            ApplyMatchState(false);
            HandleMatchFailure("매칭 요청이 서버에서 거부되었습니다.");
            return;
        }
        // success: stay in matching state, wait for OnMatchFound()
    }

    /// <summary>매칭 중일 때 매칭 버튼을 다시 누르면 호출. REQ_MATCHING_CANCEL을 보내고, UI 상태는 아직 바꾸지 않는다.</summary>
    public void RequestMatchCancel()
    {
        if (!_isMatching || _isCancelling) return;
        if (Client.Instance == null)
        {
            Debug.LogError("[LobbyUI] Client.Instance가 null입니다.");
            return;
        }

        _isCancelling = true;

        Debug.Log("[LobbyUI] Match Cancel REQ sent.");
        Client.Instance.SendMatchCancelRequest();
    }

    /// <summary>
    /// Client.OnMatchCancelResponse 콜백.
    ///   success=true  → 매칭 취소 확정. 이때만 매칭 중 상태를 원상복귀한다.
    ///   success=false → 취소 거부. 매칭 중 상태를 그대로 유지한다.
    /// </summary>
    void OnMatchCancelResponse(bool success)
    {
        _isCancelling = false;

        if (!success)
        {
            AddSystemMessage("매칭 취소 요청이 서버에서 거부되었습니다.");
            return;
        }

        _isMatching = false;
        ApplyMatchState(false);
    }

    /// <summary>Client.OnMatchSuccess 콜백.</summary>
    void HandleMatchSuccess(long opAccountNum, string opNickname) => OnMatchFound(opNickname);

    /// <summary>서버가 상대방을 확정했을 때(RES_MATCHING_SUCCESS) GameScene으로 전환한다.</summary>
    public void OnMatchFound(string opNickname)
    {
        _isMatching = false;
        ApplyMatchState(false);
        AddSystemMessage($"Match found! vs {opNickname}. Loading game...");
        SceneManager.LoadScene("GameScene");
    }

    /// <summary>매칭 실패/타임아웃 공통 처리: 확인 모달을 띄우고, 확인 시 연결을 끊고 로그인 화면으로 돌아간다.</summary>
    void HandleMatchFailure(string message)
    {
        AddSystemMessage(message);
        _confirmDialog.Show(message, () =>
        {
            Client.Instance?.Disconnect();
            SceneManager.LoadScene("LoginScene");
        });
    }

    void ApplyMatchState(bool matching)
    {
        if (_matchBtnImg != null) _matchBtnImg.color = matching ? C_BTN_MATCH : C_BTN_NORMAL;

        if (_dotAnim != null) { StopCoroutine(_dotAnim); _dotAnim = null; }

        if (matching)
            _dotAnim = StartCoroutine(DotAnimation());
        else
            if (_matchBtnTxt != null) _matchBtnTxt.text = "START MATCH";
    }

    IEnumerator DotAnimation()
    {
        string[] frames = { "MATCHING  .", "MATCHING  . .", "MATCHING  . . ." };
        int i = 0;
        while (_isMatching)
        {
            if (_matchBtnTxt != null) _matchBtnTxt.text = frames[i % 3];
            i++;
            yield return new WaitForSeconds(0.55f);
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Cache refs
    // ════════════════════════════════════════════════════════════════════

    void CacheRefs()
    {
        // LobbyUI 자신이 로비 씬의 Canvas에 붙어있는 스크립트이므로, 씬 전체에서
        // Canvas를 찾을 필요 없이 자기 transform을 그대로 root로 쓴다.
        // (FindAnyObjectByType(typeof(Canvas))는 씬에 Canvas가 여러 개면 어떤 걸 반환할지
        // 유니티 공식 문서상 "임의"라, DisconnectNoticeUI처럼 DontDestroyOnLoad로 살아남는
        // 다른 Canvas가 있으면 엉뚱한 걸 집어서 모든 참조가 null이 되는 버그가 있었다.)
        var root = transform;

        _chatScroll   = Find<ScrollRect>(root,      "ChatHistoryScroll");
        _chatContent  = Find<Transform>(root,        "ChatHistoryContent");
        _chatInput    = Find<TMP_InputField>(root,   "ChatInput");
        _sendBtn      = Find<Button>(root,           "ChatSendBtn");
        _userContent  = Find<Transform>(root,        "UserListContent");
        _matchBtn     = Find<Button>(root,           "MatchBtn");
        _matchBtnImg  = _matchBtn?.GetComponent<Image>();
        _matchBtnTxt  = Find<TextMeshProUGUI>(root,  "MatchBtnText");
    }

    static T Find<T>(Transform root, string n) where T : Component
    {
        foreach (T c in root.GetComponentsInChildren<T>(true))
            if (c.gameObject.name == n) return c;
        return null;
    }

    static string ToHex(Color c)
        => string.Format("{0:X2}{1:X2}{2:X2}",
            (int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255));
}
