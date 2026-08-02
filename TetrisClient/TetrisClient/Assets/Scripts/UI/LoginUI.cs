using UnityEngine;
using UnityEngine.UI;
using TMPro;

/// <summary>
/// Manages the Login Scene: login form + sign-up popup.
/// Finds all UI elements by name at Start() – no Inspector wiring needed.
/// [SERVER_HOOK] marks integration points for the network layer.
/// </summary>
public class LoginUI : MonoBehaviour
{
    // ── Login form ───────────────────────────────────────────────────────
    TMP_InputField  _loginId;
    TMP_InputField  _loginPw;
    Button          _loginBtn;
    Button          _signUpBtn;

    // ── State ────────────────────────────────────────────────────────────
    bool _idChecked = false;
    bool _nickChecked = false;

    // ── Sign-up popup ────────────────────────────────────────────────────
    GameObject      _popup;
    TMP_InputField  _suId;
    TMP_InputField  _suPw;
    TMP_InputField  _suNick;
    Button          _checkIdBtn;
    Button          _checkNickBtn;
    Button          _confirmBtn;
    Button          _cancelBtn;
    TextMeshProUGUI _statusText;

    // ════════════════════════════════════════════════════════════════════
    void Start()
    {
        CacheRefs();

        _loginBtn?.onClick.AddListener(OnLogin);
        _signUpBtn?.onClick.AddListener(OpenPopup);
        _checkIdBtn?.onClick.AddListener(OnCheckId);
        _checkNickBtn?.onClick.AddListener(OnCheckNickname);
        _confirmBtn?.onClick.AddListener(OnConfirmSignUp);
        _cancelBtn?.onClick.AddListener(ClosePopup);

        if (_popup != null) _popup.SetActive(false);
    }

    // ════════════════════════════════════════════════════════════════════
    // Popup open / close
    // ════════════════════════════════════════════════════════════════════
    public void OpenPopup()
    {
        if (_popup == null) return;
        _idChecked   = false;
        _nickChecked = false;
        ClearPopupInputs();
        SetStatus("");
        _popup.SetActive(true);
    }

    public void ClosePopup()
    {
        if (_popup != null) _popup.SetActive(false);
    }

    // ════════════════════════════════════════════════════════════════════
    // Login
    // ════════════════════════════════════════════════════════════════════
    void OnLogin()
    {
        string id = _loginId != null ? _loginId.text.Trim() : "";
        string pw = _loginPw != null ? _loginPw.text        : "";

        if (string.IsNullOrEmpty(id)) { Debug.LogWarning("[LoginUI] ID is empty.");       return; }
        if (string.IsNullOrEmpty(pw)) { Debug.LogWarning("[LoginUI] Password is empty."); return; }

        Debug.Log("[LoginUI] Login → ID: " + id);
        // [SERVER_HOOK] NetworkManager.Instance.SendLogin(id, pw);
    }

    // ════════════════════════════════════════════════════════════════════
    // Sign-up popup handlers
    // ════════════════════════════════════════════════════════════════════
    void OnCheckId()
    {
        string id = _suId != null ? _suId.text.Trim() : "";
        if (string.IsNullOrEmpty(id))
        {
            SetStatus("Please enter an ID first.");
            return;
        }
        _idChecked = false;
        SetStatus("Checking ID availability...");
        Debug.Log("[LoginUI] CheckId: " + id);
        // [SERVER_HOOK] NetworkManager.Instance.CheckId(id, OnCheckIdResult);

        // ── Stub (remove when server is connected) ──
        OnCheckIdResult(true);
    }

    void OnCheckNickname()
    {
        string nick = _suNick != null ? _suNick.text.Trim() : "";
        if (string.IsNullOrEmpty(nick))
        {
            SetStatus("Please enter a nickname first.");
            return;
        }
        _nickChecked = false;
        SetStatus("Checking nickname availability...");
        Debug.Log("[LoginUI] CheckNickname: " + nick);
        // [SERVER_HOOK] NetworkManager.Instance.CheckNickname(nick, OnCheckNicknameResult);

        // ── Stub (remove when server is connected) ──
        OnCheckNicknameResult(true);
    }

    void OnConfirmSignUp()
    {
        string id   = _suId   != null ? _suId.text.Trim()   : "";
        string pw   = _suPw   != null ? _suPw.text          : "";
        string nick = _suNick != null ? _suNick.text.Trim() : "";

        if (string.IsNullOrEmpty(id))   { SetStatus("ID cannot be empty.");       return; }
        if (string.IsNullOrEmpty(pw))   { SetStatus("Password cannot be empty."); return; }
        if (string.IsNullOrEmpty(nick)) { SetStatus("Nickname cannot be empty."); return; }
        if (!_idChecked)   { SetStatus("Please check ID availability first.");       return; }
        if (!_nickChecked) { SetStatus("Please check nickname availability first."); return; }

        Debug.Log("[LoginUI] SignUp → ID: " + id + "  Nick: " + nick);
        SetStatus("Creating account...");
        // [SERVER_HOOK] NetworkManager.Instance.SendSignUp(id, pw, nick);
    }

    // ════════════════════════════════════════════════════════════════════
    // Server response callbacks  [SERVER_HOOK] – call these from network layer
    // ════════════════════════════════════════════════════════════════════
    public void OnCheckIdResult(bool available)
    {
        _idChecked = available;
        SetStatus(available ? "✓  ID is available." : "✗  ID is already taken.");
    }

    public void OnCheckNicknameResult(bool available)
    {
        _nickChecked = available;
        SetStatus(available ? "✓  Nickname is available." : "✗  Nickname is already taken.");
    }

    public void OnSignUpSuccess()
    {
        SetStatus("Account created!  You can now log in.");
        Invoke(nameof(ClosePopup), 1.5f);
    }

    public void OnSignUpFailed(string reason)
        => SetStatus("Sign up failed: " + reason);

    // ════════════════════════════════════════════════════════════════════
    // Helpers
    // ════════════════════════════════════════════════════════════════════
    void SetStatus(string msg)
    {
        if (_statusText != null) _statusText.text = msg;
    }

    void ClearPopupInputs()
    {
        if (_suId)   _suId.text   = "";
        if (_suPw)   _suPw.text   = "";
        if (_suNick) _suNick.text = "";
    }

    void CacheRefs()
    {
        _loginId   = Find<TMP_InputField>("IDInput");
        _loginPw   = Find<TMP_InputField>("PWInput");
        _loginBtn  = Find<Button>("LoginBtn");
        _signUpBtn = Find<Button>("SignUpBtn");

        _popup = GameObject.Find("SignUpPopup");
        if (_popup != null)
        {
            _suId        = FindInTree<TMP_InputField>(_popup, "SUIDInput");
            _suPw        = FindInTree<TMP_InputField>(_popup, "SUPWInput");
            _suNick      = FindInTree<TMP_InputField>(_popup, "SUNickInput");
            _checkIdBtn  = FindInTree<Button>(_popup, "CheckIDBtn");
            _checkNickBtn= FindInTree<Button>(_popup, "CheckNickBtn");
            _confirmBtn  = FindInTree<Button>(_popup, "ConfirmBtn");
            _cancelBtn   = FindInTree<Button>(_popup, "CancelBtn");
            _statusText  = FindInTree<TextMeshProUGUI>(_popup, "StatusText");
        }
    }

    static T Find<T>(string name) where T : Component
    {
        var go = GameObject.Find(name);
        return go != null ? go.GetComponent<T>() : null;
    }

    static T FindInTree<T>(GameObject root, string name) where T : Component
    {
        foreach (T c in root.GetComponentsInChildren<T>(true))
            if (c.gameObject.name == name) return c;
        return null;
    }
}
