using UnityEngine;
using UnityEngine.UI;
using TMPro;

/// <summary>
/// Manages the Login Scene: login form, sign-up popup, and toast notifications.
/// [SERVER_HOOK] marks network integration points.
/// </summary>
public class LoginUI : MonoBehaviour
{
    // ── Login form ───────────────────────────────────────────────────────
    TMP_InputField   _loginId;
    TMP_InputField   _loginPw;
    Button           _loginBtn;
    Button           _signUpBtn;

    // ── Sign-up popup ────────────────────────────────────────────────────
    GameObject       _signUpPopup;
    TMP_InputField   _suId;
    TMP_InputField   _suPw;
    TMP_InputField   _suNick;
    Button           _checkIdBtn;
    Button           _checkNickBtn;
    Button           _confirmBtn;
    Button           _cancelBtn;
    TextMeshProUGUI  _statusText;
    bool             _idChecked;
    bool             _nickChecked;

    // ── Toast notification ───────────────────────────────────────────────
    GameObject       _toast;
    Image            _toastBorderImg;
    Image            _toastStripImg;
    TextMeshProUGUI  _toastIcon;
    TextMeshProUGUI  _toastTitle;
    TextMeshProUGUI  _toastMessage;
    Button           _toastOKBtn;

    static readonly Color COL_SUCCESS = new Color(0.30f, 0.85f, 0.50f, 1f);
    static readonly Color COL_ERROR   = new Color(0.90f, 0.32f, 0.32f, 1f);
    static readonly Color COL_INFO    = new Color(0.40f, 0.65f, 1.00f, 1f);

    public enum ToastType { Info, Success, Error }

    // ════════════════════════════════════════════════════════════════════
    void Start()
    {
        CacheRefs();

        _loginBtn?.onClick.AddListener(OnLogin);
        _signUpBtn?.onClick.AddListener(OpenSignUp);
        _checkIdBtn?.onClick.AddListener(OnCheckId);
        _checkNickBtn?.onClick.AddListener(OnCheckNickname);
        _confirmBtn?.onClick.AddListener(OnConfirmSignUp);
        _cancelBtn?.onClick.AddListener(CloseSignUp);
        _toastOKBtn?.onClick.AddListener(CloseToast);

        if (_signUpPopup != null) _signUpPopup.SetActive(false);
        if (_toast       != null) _toast.SetActive(false);
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Toast (public API) ───────────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    /// <summary>Display a toast notification. Call from anywhere.</summary>
    public void ShowToast(string message, ToastType type = ToastType.Info)
    {
        if (_toast == null) { Debug.LogWarning("[LoginUI] ToastPopup not found."); return; }

        Color  col   = type == ToastType.Success ? COL_SUCCESS :
                       type == ToastType.Error   ? COL_ERROR   : COL_INFO;
        string icon  = type == ToastType.Success ? "OK" :
                       type == ToastType.Error   ? "!!" : "i";
        string title = type == ToastType.Success ? "SUCCESS" :
                       type == ToastType.Error   ? "ERROR"   : "INFO";

        if (_toastBorderImg != null) _toastBorderImg.color = col;
        if (_toastStripImg  != null) _toastStripImg.color  = col;
        if (_toastIcon      != null) { _toastIcon.text  = icon;  _toastIcon.color  = col; }
        if (_toastTitle     != null) { _toastTitle.text = title; _toastTitle.color = col; }
        if (_toastMessage   != null)   _toastMessage.text = message;

        _toast.SetActive(true);
    }

    /// <summary>Hide the toast notification.</summary>
    public void CloseToast()
    {
        if (_toast != null) _toast.SetActive(false);
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Sign-up popup ────────────────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    public void OpenSignUp()
    {
        if (_signUpPopup == null) return;
        _idChecked = false; _nickChecked = false;
        ClearSignUpFields(); SetStatus("");
        _signUpPopup.SetActive(true);
    }

    public void CloseSignUp()
    {
        if (_signUpPopup != null) _signUpPopup.SetActive(false);
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Button handlers ──────────────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    void OnLogin()
    {
        string id = _loginId != null ? _loginId.text.Trim() : "";
        string pw = _loginPw != null ? _loginPw.text        : "";
        if (string.IsNullOrEmpty(id)) { ShowToast("Please enter your ID.",       ToastType.Error); return; }
        if (string.IsNullOrEmpty(pw)) { ShowToast("Please enter your password.", ToastType.Error); return; }
        Debug.Log("[LoginUI] Login → " + id);
        // [SERVER_HOOK] NetworkManager.Instance.SendLogin(id, pw);
    }

    void OnCheckId()
    {
        string id = _suId != null ? _suId.text.Trim() : "";
        if (string.IsNullOrEmpty(id)) { ShowToast("Enter an ID to check.", ToastType.Error); return; }
        SetStatus("Checking...");
        // [SERVER_HOOK] NetworkManager.Instance.CheckId(id, OnCheckIdResult);
        OnCheckIdResult(true); // stub – remove when server connected
    }

    void OnCheckNickname()
    {
        string nick = _suNick != null ? _suNick.text.Trim() : "";
        if (string.IsNullOrEmpty(nick)) { ShowToast("Enter a nickname to check.", ToastType.Error); return; }
        SetStatus("Checking...");
        // [SERVER_HOOK] NetworkManager.Instance.CheckNickname(nick, OnCheckNicknameResult);
        OnCheckNicknameResult(true); // stub
    }

    void OnConfirmSignUp()
    {
        string id   = _suId   != null ? _suId.text.Trim()   : "";
        string pw   = _suPw   != null ? _suPw.text          : "";
        string nick = _suNick != null ? _suNick.text.Trim() : "";

        if (string.IsNullOrEmpty(id))   { ShowToast("ID cannot be empty.",                      ToastType.Error); return; }
        if (string.IsNullOrEmpty(pw))   { ShowToast("Password cannot be empty.",                ToastType.Error); return; }
        if (string.IsNullOrEmpty(nick)) { ShowToast("Nickname cannot be empty.",                ToastType.Error); return; }
        if (!_idChecked)                { ShowToast("Check ID availability first.",             ToastType.Error); return; }
        if (!_nickChecked)              { ShowToast("Check nickname availability first.",       ToastType.Error); return; }

        SetStatus("Creating account...");
        Debug.Log("[LoginUI] SignUp → " + id + " / " + nick);
        // [SERVER_HOOK] NetworkManager.Instance.SendSignUp(id, pw, nick);
    }

    // ════════════════════════════════════════════════════════════════════
    // ── Server callbacks  [SERVER_HOOK] ──────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    public void OnLoginFailed(string reason)
        => ShowToast("Login failed: " + reason, ToastType.Error);

    public void OnCheckIdResult(bool available)
    {
        _idChecked = available;
        SetStatus(available ? "ID is available." : "ID is already taken.");
        ShowToast(
            available ? "This ID is available!" : "This ID is already taken.",
            available ? ToastType.Success : ToastType.Error);
    }

    public void OnCheckNicknameResult(bool available)
    {
        _nickChecked = available;
        SetStatus(available ? "Nickname is available." : "Nickname is already taken.");
        ShowToast(
            available ? "This nickname is available!" : "This nickname is already taken.",
            available ? ToastType.Success : ToastType.Error);
    }

    public void OnSignUpSuccess()
    {
        CloseSignUp();
        ShowToast("Account created!  You can now log in.", ToastType.Success);
    }

    public void OnSignUpFailed(string reason)
        => ShowToast("Sign up failed: " + reason, ToastType.Error);

    // ════════════════════════════════════════════════════════════════════
    // ── Helpers ──────────────────────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    void SetStatus(string msg) { if (_statusText != null) _statusText.text = msg; }

    void ClearSignUpFields()
    {
        if (_suId   != null) _suId.text   = "";
        if (_suPw   != null) _suPw.text   = "";
        if (_suNick != null) _suNick.text = "";
    }

    void CacheRefs()
    {
        Canvas cv = FindObjectOfType<Canvas>();

        // Active objects – GameObject.Find works fine
        _loginId   = FindComp<TMP_InputField>("IDInput");
        _loginPw   = FindComp<TMP_InputField>("PWInput");
        _loginBtn  = FindComp<Button>("LoginBtn");
        _signUpBtn = FindComp<Button>("SignUpBtn");

        if (cv == null) return;

        // Inactive objects – Transform.Find works on inactive children
        Transform suRoot = cv.transform.Find("SignUpPopup");
        if (suRoot != null)
        {
            _signUpPopup  = suRoot.gameObject;
            _suId         = DeepFind<TMP_InputField>(suRoot, "SUIDInput");
            _suPw         = DeepFind<TMP_InputField>(suRoot, "SUPWInput");
            _suNick       = DeepFind<TMP_InputField>(suRoot, "SUNickInput");
            _checkIdBtn   = DeepFind<Button>(suRoot, "CheckIDBtn");
            _checkNickBtn = DeepFind<Button>(suRoot, "CheckNickBtn");
            _confirmBtn   = DeepFind<Button>(suRoot, "ConfirmBtn");
            _cancelBtn    = DeepFind<Button>(suRoot, "CancelBtn");
            _statusText   = DeepFind<TextMeshProUGUI>(suRoot, "StatusText");
        }

        Transform toastRoot = cv.transform.Find("ToastPopup");
        if (toastRoot != null)
        {
            _toast          = toastRoot.gameObject;
            _toastBorderImg = DeepFind<Image>(toastRoot,           "ToastBorder");
            _toastStripImg  = DeepFind<Image>(toastRoot,           "ToastStrip");
            _toastIcon      = DeepFind<TextMeshProUGUI>(toastRoot, "ToastIcon");
            _toastTitle     = DeepFind<TextMeshProUGUI>(toastRoot, "ToastTitle");
            _toastMessage   = DeepFind<TextMeshProUGUI>(toastRoot, "ToastMessage");
            _toastOKBtn     = DeepFind<Button>(toastRoot,          "ToastOKBtn");
        }
    }

    static T FindComp<T>(string goName) where T : Component
    { var g = GameObject.Find(goName); return g != null ? g.GetComponent<T>() : null; }

    static T DeepFind<T>(Transform root, string name) where T : Component
    {
        foreach (T c in root.GetComponentsInChildren<T>(true))
            if (c.gameObject.name == name) return c;
        return null;
    }
}
