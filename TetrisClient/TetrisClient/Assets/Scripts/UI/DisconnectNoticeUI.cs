using UnityEngine;
using UnityEngine.UI;
using UnityEngine.SceneManagement;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using TMPro;

/// <summary>
/// 서버가 세션을 강제로 끊었을 때(Disconnect) 로비/게임 어디에 있든 공통으로
/// "연결이 끊겼습니다!" 안내를 화면 전체 마스크와 함께 띄우고, 확인을 누르면
/// 로그인 씬으로 돌려보낸다.
/// </summary>
public class DisconnectNoticeUI : MonoBehaviour
{
    public static DisconnectNoticeUI Instance { get; private set; }

    [Header("씬에 미리 배치된 자식 오브젝트 연결")]
    [SerializeField] GameObject _panel;            // Mask + Message + ConfirmBtn을 담는 자식, 기본 비활성
    [SerializeField] TextMeshProUGUI _messageText;
    [SerializeField] Button _confirmBtn;

    void Awake()
    {
        // Client.cs와 동일한 싱글톤 패턴: 연결 종료로 LoginScene이 다시 로드되면
        // 씬에 배치된 원본 오브젝트가 또 하나 생기려 하므로, 기존 인스턴스가
        // 있으면 새로 생긴 쪽을 파괴한다.
        if (Instance != null && Instance != this) { Destroy(gameObject); return; }
        Instance = this;
        DontDestroyOnLoad(gameObject);

        if (_panel == null || _messageText == null || _confirmBtn == null)
        {
            Debug.LogError("[DisconnectNoticeUI] _panel / _messageText / _confirmBtn이 Inspector에 연결되어 있지 않습니다.");
            return;
        }

        _confirmBtn.onClick.AddListener(OnConfirmClicked);
        _panel.SetActive(false);
    }

    void Start()
    {
        // Client도 LoginScene의 씬 루트 오브젝트라 Awake 호출 순서가 보장되지 않는다.
        // 같은 씬의 모든 Awake는 어떤 Start보다도 먼저 끝나는 게 보장되므로 여기서 구독한다.
        if (Client.Instance != null)
            Client.Instance.OnDisconnected += HandleDisconnected;
        else
            Debug.LogError("[DisconnectNoticeUI] Client.Instance가 아직 없습니다. LoginScene이 첫 번째 씬인지 확인해주세요.");
    }

    void OnDestroy()
    {
        if (Client.Instance != null)
            Client.Instance.OnDisconnected -= HandleDisconnected;
    }

    /// <summary>
    /// Client.OnDisconnected 콜백. 로그인 화면은 LoginSceneManager의 기존 토스트가
    /// 이미 처리하고 있으므로, 여긴 그 외(로비/게임)에서만 반응한다.
    /// </summary>
    void HandleDisconnected(string reason)
    {
        if (SceneManager.GetActiveScene().name == "LoginScene")
            return;

        EnsureEventSystem();

        _messageText.text = "연결이 끊겼습니다!";
        _panel.SetActive(true);
    }

    void OnConfirmClicked()
    {
        _panel.SetActive(false);
        Client.Instance?.Disconnect();
        SceneManager.LoadScene("LoginScene");
    }

    /// <summary>GameScene은 씬 자체에 EventSystem이 없어서(GameResultUI.cs와 동일한 이유), 없으면 하나 만든다.</summary>
    static void EnsureEventSystem()
    {
        if (EventSystem.current != null) return;

        new GameObject("EventSystem", typeof(EventSystem), typeof(InputSystemUIInputModule));
    }
}
