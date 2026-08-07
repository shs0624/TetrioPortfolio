using UnityEngine;
using UnityEngine.UI;
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
    Coroutine       _dotAnim;

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

        _sendBtn?.onClick.AddListener(OnSendChat);
        _matchBtn?.onClick.AddListener(OnMatchBtnClick);

        if (_chatInput != null)
            _chatInput.onSubmit.AddListener(_ => OnSendChat());

        AddSystemMessage("Welcome to the lobby!");
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

        Debug.Log("[LobbyUI] Chat → " + msg);
        // [SERVER_HOOK] NetworkManager.Instance.SendChat(msg);
    }

    /// <summary>[SERVER_HOOK] Received a chat message from server.</summary>
    public void OnChatReceived(string author, string content)
        => AppendChat(author, content, C_AUTHOR, C_MSG);

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
        tmp.text = string.Format(
            "<color=#{0}><b>[{1}]</b></color>  <color=#{2}>{3}</color>",
            ToHex(authorCol), author, ToHex(textCol), content);
        tmp.fontSize           = 14f;
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

    /// <summary>[SERVER_HOOK] A player entered the lobby.</summary>
    public void AddUser(string username, int level)
    {
        if (_userContent == null || _users.ContainsKey(username)) return;

        var go  = new GameObject("User_" + username);
        go.transform.SetParent(_userContent, false);

        var le  = go.AddComponent<LayoutElement>();
        le.preferredHeight = 34f;
        le.flexibleWidth   = 1f;

        var tmp = go.AddComponent<TextMeshProUGUI>();
        tmp.text = string.Format(
            "<color=#{0}><b>Lv.{1}</b></color>   {2}",
            ToHex(C_LEVEL), level, username);
        tmp.fontSize      = 15f;
        tmp.color         = C_MSG;
        tmp.font          = _font;
        tmp.alignment     = TextAlignmentOptions.Left;
        tmp.raycastTarget = false;

        _users[username] = go;
    }

    /// <summary>[SERVER_HOOK] A player left the lobby.</summary>
    public void RemoveUser(string username)
    {
        if (!_users.TryGetValue(username, out var go)) return;
        Destroy(go);
        _users.Remove(username);
    }

    /// <summary>[SERVER_HOOK] Full user-list snapshot from server.</summary>
    public void SetUserList(string[] names, int[] levels)
    {
        foreach (var e in _users.Values) Destroy(e);
        _users.Clear();
        for (int i = 0; i < names.Length; i++)
            AddUser(names[i], i < levels.Length ? levels[i] : 1);
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Matching
    // ════════════════════════════════════════════════════════════════════

    void OnMatchBtnClick()
    {
        if (_isMatching) return;   // button is locked while matching
        RequestMatch();
    }

    public void RequestMatch()
    {
        if (_isMatching) return;
        _isMatching = true;
        ApplyMatchState(true);

        Debug.Log("[LobbyUI] Match REQ sent.");
        // [SERVER_HOOK] NetworkManager.Instance.SendMatchRequest();
    }

    /// <summary>
    /// [SERVER_HOOK] Call with the server's match RES.
    ///   success=true  → keep "matching…" state, wait for OnMatchFound().
    ///   success=false → revert button to normal.
    /// </summary>
    public void OnMatchResponse(bool success)
    {
        if (!success)
        {
            _isMatching = false;
            ApplyMatchState(false);
            AddSystemMessage("Match request rejected by server.");
        }
        // success: stay in matching state
    }

    /// <summary>[SERVER_HOOK] Server found an opponent, load game scene.</summary>
    public void OnMatchFound()
    {
        _isMatching = false;
        ApplyMatchState(false);
        AddSystemMessage("Match found! Loading game...");
        // [SERVER_HOOK] SceneManager.LoadScene("GameScene");
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
        var cv = (Canvas)Object.FindAnyObjectByType(typeof(Canvas));
        if (cv == null) return;

        var root = cv.transform;

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
