using System;
using System.Collections;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;
using TMPro;

/// <summary>
/// LoginScene의 UI를 관리하는 컨트롤러.
///
/// ── 동작 흐름 ────────────────────────────────────────────────────
///  1) 네트워크 버튼 클릭 시 서버가 연결되어 있지 않으면 ConnectToLoginServer()를
///     자동 호출한다. (lazy connect)
///  2) 연결 완료 후 해당 패킷을 전송한다.
///  3) 로그인 서버 → 게임 서버 연결까지 성공하면 OnGameServerConnected 이벤트가
///     발생하고 LobbyScene으로 전환한다.
///  4) 오류/실패 메시지는 모두 ToastUI를 통해 출력한다.
///
/// ── Inspector 연결 필수 항목 ─────────────────────────────────────
///  Login Panel    : loginPanel, loginIdInput, loginPwInput,
///                   loginBtn, toRegisterBtn
///  Register Panel : registerPanel, regIdInput, regPwInput, regNickInput,
///                   checkIdBtn, checkNickBtn, registerBtn, toLoginBtn
///  Toast          : toast  (ToastUI 컴포넌트)
///  Scene          : lobbySceneName (기본값 "LobbyScene")
///
/// ── 버튼 OnClick 연결 목록 ──────────────────────────────────────
///  loginBtn       → OnLoginClicked()
///  toRegisterBtn  → OnToRegisterClicked()
///  checkIdBtn     → OnCheckIdClicked()
///  checkNickBtn   → OnCheckNickClicked()
///  registerBtn    → OnRegisterClicked()
///  toLoginBtn     → OnToLoginClicked()
/// </summary>
public class LoginSceneManager : MonoBehaviour
{
    // ── 로그인 패널 ────────────────────────────────────────────────
    [Header("Login Panel")]
    [SerializeField] GameObject     loginPanel;
    [SerializeField] TMP_InputField loginIdInput;
    [SerializeField] TMP_InputField loginPwInput;
    [SerializeField] Button         loginBtn;
    [SerializeField] Button         toRegisterBtn;

    // ── 회원가입 패널 ──────────────────────────────────────────────
    [Header("Register Panel")]
    [SerializeField] GameObject     registerPanel;
    [SerializeField] TMP_InputField regIdInput;
    [SerializeField] TMP_InputField regPwInput;
    [SerializeField] TMP_InputField regNickInput;
    [SerializeField] Button         checkIdBtn;
    [SerializeField] Button         checkNickBtn;
    [SerializeField] Button         registerBtn;
    [SerializeField] Button         toLoginBtn;

    // ── Toast ─────────────────────────────────────────────────────
    [Header("Toast")]
    [SerializeField] ToastUI toast;

    // ── 씬 설정 ────────────────────────────────────────────────────
    [Header("Scene")]
    [SerializeField] string lobbySceneName = "LobbyScene";

    // ── 내부 상태 ──────────────────────────────────────────────────
    bool _idChecked;
    bool _nickChecked;

    const float CONNECT_TIMEOUT   = 5f;
    const float TOAST_ERROR_SEC   = 4f;   // 오류 메시지 자동 닫힘
    const float TOAST_SUCCESS_SEC = 2f;   // 성공 메시지 자동 닫힘
    const float TOAST_PERSIST     = 0f;   // 자동 닫힘 없음 (중복확인 결과 등)

    // ══════════════════════════════════════════════════════════════
    // Unity Lifecycle
    // ══════════════════════════════════════════════════════════════

    void Start()
    {
        if (Client.Instance == null)
        {
            Debug.LogError(
                "[LoginSceneManager] Client.Instance가 null입니다.\n" +
                "LoginScene 하이어라키에 Client 컴포넌트가 붙은 GameObject를 추가해주세요.");
            return;
        }

        Client.Instance.OnGameServerConnected += HandleGameServerConnected;
        Client.Instance.OnDisconnected        += HandleDisconnected;

        ShowLoginPanel();

        // LoginUI.Start()도 같은 ToastPopup을 끄므로, 모든 Start()가 끝난 다음 프레임에 띄운다.
        if (Client.Instance.ConfigError != null)
            StartCoroutine(DelayThen(0f, () => toast.Show($"config.txt 오류: {Client.Instance.ConfigError}", 0f)));
    }

    void OnDestroy()
    {
        if (Client.Instance == null) return;
        Client.Instance.OnGameServerConnected -= HandleGameServerConnected;
        Client.Instance.OnDisconnected        -= HandleDisconnected;
    }

    // ══════════════════════════════════════════════════════════════
    // 패널 전환
    // ══════════════════════════════════════════════════════════════

    void ShowLoginPanel()
    {
        loginPanel.SetActive(true);
        registerPanel.SetActive(false);
        toast.Hide();
        SetLoginPanelInteractable(true);
    }

    void ShowRegisterPanel()
    {
        loginPanel.SetActive(false);
        registerPanel.SetActive(true);
        toast.Hide();
        SetRegisterPanelInteractable(true);
        _idChecked   = false;
        _nickChecked = false;
    }

    // ══════════════════════════════════════════════════════════════
    // 버튼 이벤트 핸들러
    // ══════════════════════════════════════════════════════════════

    /// <summary>로그인 버튼</summary>
    public void OnLoginClicked()
    {
        string id = loginIdInput.text.Trim();
        string pw = loginPwInput.text;

        if (string.IsNullOrEmpty(id) || string.IsNullOrEmpty(pw))
        {
            toast.Show("아이디와 비밀번호를 입력해주세요.");
            return;
        }

        SetLoginPanelInteractable(false);
        toast.Hide();

        StartCoroutine(ConnectThenDo(() =>
        {
            Client.Instance.Login(id, pw, (ok, err) =>
            {
                if (ok)
                {
                    // 게임 서버 연결 완료는 OnGameServerConnected 에서 처리
                }
                else
                {
                    string msg = string.IsNullOrEmpty(err)
                        ? "로그인 실패 (아이디 또는 비밀번호를 확인해주세요)"
                        : err;
                    toast.Show(msg);
                    SetLoginPanelInteractable(true);
                }
            });
        }));
    }

    /// <summary>회원가입 화면으로 이동</summary>
    public void OnToRegisterClicked() => ShowRegisterPanel();

    /// <summary>로그인 화면으로 이동</summary>
    public void OnToLoginClicked() => ShowLoginPanel();

    /// <summary>아이디 중복 확인 버튼</summary>
    public void OnCheckIdClicked()
    {
        string id = regIdInput.text.Trim();
        if (string.IsNullOrEmpty(id))
        {
            toast.Show("아이디를 입력해주세요.");
            return;
        }

        _idChecked = false;
        SetRegisterPanelInteractable(false);

        StartCoroutine(ConnectThenDo(() =>
        {
            string nick = regNickInput.text.Trim();
            Client.Instance.CheckId(id, nick, available =>
            {
                _idChecked = available;
                // 중복 확인 결과는 사용자가 직접 닫기 전까지 유지
                toast.Show(
                    available ? "✓ 사용 가능한 아이디입니다." : "✗ 이미 사용 중인 아이디입니다.",
                    TOAST_PERSIST);
                SetRegisterPanelInteractable(true);
            });
        }));
    }

    /// <summary>닉네임 중복 확인 버튼</summary>
    public void OnCheckNickClicked()
    {
        string id   = regIdInput.text.Trim();
        string nick = regNickInput.text.Trim();

        if (string.IsNullOrEmpty(nick))
        {
            toast.Show("닉네임을 입력해주세요.", TOAST_ERROR_SEC);
            return;
        }

        _nickChecked = false;
        SetRegisterPanelInteractable(false);

        StartCoroutine(ConnectThenDo(() =>
        {
            Client.Instance.CheckNickname(id, nick, available =>
            {
                _nickChecked = available;
                toast.Show(
                    available ? "✓ 사용 가능한 닉네임입니다." : "✗ 이미 사용 중인 닉네임입니다.",
                    TOAST_PERSIST);
                SetRegisterPanelInteractable(true);
            });
        }));
    }

    /// <summary>회원가입 버튼</summary>
    public void OnRegisterClicked()
    {
        if (!_idChecked)
        {
            toast.Show("아이디 중복 확인을 먼저 해주세요.");
            return;
        }
        if (!_nickChecked)
        {
            toast.Show("닉네임 중복 확인을 먼저 해주세요.");
            return;
        }

        string id   = regIdInput.text.Trim();
        string pw   = regPwInput.text;
        string nick = regNickInput.text.Trim();

        if (string.IsNullOrEmpty(id) || string.IsNullOrEmpty(pw) || string.IsNullOrEmpty(nick))
        {
            toast.Show("모든 항목을 입력해주세요.");
            return;
        }

        SetRegisterPanelInteractable(false);

        StartCoroutine(ConnectThenDo(() =>
        {
            Client.Instance.Register(id, pw, nick, ok =>
            {
                if (ok)
                {
                    toast.Show("회원가입 성공! 로그인 화면으로 이동합니다.");
                    StartCoroutine(DelayThen(TOAST_SUCCESS_SEC, ShowLoginPanel));
                }
                else
                {
                    toast.Show("회원가입 실패. 다시 시도해주세요.");
                    SetRegisterPanelInteractable(true);
                }
            });
        }));
    }

    // ══════════════════════════════════════════════════════════════
    // Client 이벤트 핸들러
    // ══════════════════════════════════════════════════════════════

    void HandleGameServerConnected()
    {
        SceneManager.LoadScene(lobbySceneName);
    }

    void HandleDisconnected(string reason)
    {
        string msg = string.IsNullOrEmpty(reason)
            ? "서버 연결이 끊겼습니다."
            : $"연결 끊김: {reason}";

        toast.Show(msg);
        SetBothPanelsInteractable(true);
    }

    // ══════════════════════════════════════════════════════════════
    // 공통 유틸
    // ══════════════════════════════════════════════════════════════

    /// <summary>
    /// 로그인 서버 연결을 보장한 뒤 action을 실행하는 코루틴.
    ///
    ///  Idle            → ConnectToLoginServer() 호출 후 완료 대기
    ///  LoginConnecting → 완료될 때까지 대기 (다른 버튼이 먼저 연결 중인 경우)
    ///  LoginReady      → 즉시 action 실행
    /// </summary>
    IEnumerator ConnectThenDo(Action action)
    {
        // Idle: 연결 시작
        if (Client.Instance.State == Client.NetState.Idle)
        {
            string connectError = null;
            bool   connected    = false;

            Client.Instance.ConnectToLoginServer(
                onConnected: () => connected     = true,
                onError:     msg => connectError = msg
            );

            float elapsed = 0f;
            while (!connected && connectError == null)
            {
                elapsed += Time.deltaTime;
                if (elapsed >= CONNECT_TIMEOUT)
                {
                    toast.Show("서버 연결 시간 초과. 다시 시도해주세요.");
                    SetBothPanelsInteractable(true);
                    yield break;
                }
                yield return null;
            }

            if (connectError != null)
            {
                toast.Show($"연결 오류: {connectError}");
                SetBothPanelsInteractable(true);
                yield break;
            }
        }

        // LoginConnecting: 다른 요청이 이미 연결을 시작한 경우 완료 대기
        else if (Client.Instance.State == Client.NetState.LoginConnecting)
        {
            float elapsed = 0f;
            while (Client.Instance.State == Client.NetState.LoginConnecting)
            {
                elapsed += Time.deltaTime;
                if (elapsed >= CONNECT_TIMEOUT)
                {
                    toast.Show("서버 연결 시간 초과. 다시 시도해주세요.");
                    SetBothPanelsInteractable(true);
                    yield break;
                }
                yield return null;
            }

            if (Client.Instance.State != Client.NetState.LoginReady)
            {
                toast.Show("연결 실패. 다시 시도해주세요.");
                SetBothPanelsInteractable(true);
                yield break;
            }
        }

        // LoginReady: 즉시 실행
        action?.Invoke();
    }

    IEnumerator DelayThen(float delay, Action action)
    {
        yield return new WaitForSeconds(delay);
        action?.Invoke();
    }

    void SetLoginPanelInteractable(bool v)
    {
        if (loginBtn      != null) loginBtn.interactable      = v;
        if (toRegisterBtn != null) toRegisterBtn.interactable = v;
    }

    void SetRegisterPanelInteractable(bool v)
    {
        if (checkIdBtn   != null) checkIdBtn.interactable   = v;
        if (checkNickBtn != null) checkNickBtn.interactable = v;
        if (registerBtn  != null) registerBtn.interactable  = v;
        if (toLoginBtn   != null) toLoginBtn.interactable   = v;
    }

    void SetBothPanelsInteractable(bool v)
    {
        SetLoginPanelInteractable(v);
        SetRegisterPanelInteractable(v);
    }
}
