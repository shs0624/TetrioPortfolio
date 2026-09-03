using UnityEngine;
using UnityEngine.UI;
using UnityEngine.SceneManagement;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using TMPro;

/// <summary>
/// 승패 결과를 화면 전체에 반투명 검은 마스크로 덮고, 중앙에 결과 텍스트와
/// 로비로 돌아가는 버튼을 띄운다. 예전에는 Canvas를 포함한 UI 전체를 런타임에
/// 코드로 즉석 생성했지만, 그러면 씬에 실제로 존재하지 않아 Inspector에서
/// TextMeshPro 폰트를 바꿀 수 없었다(한글 깨짐 원인). 그래서 이 오브젝트 자체를
/// GameScene에 미리 배치해두고(기본 비활성) 코드는 SetActive로만 켜고 끈다 —
/// 자식 텍스트/버튼은 Inspector로 연결한다.
/// GameScene에는 (지금까지 클릭이 필요한 UI가 없어서) EventSystem이 없으므로,
/// 버튼 클릭이 실제로 동작하도록 없으면 직접 하나 만들어둔다.
/// </summary>
public class GameResultUI : MonoBehaviour
{
    const string LOBBY_SCENE_NAME = "LobbyScene";

    static readonly Color WIN_COLOR  = new Color(0.30f, 0.85f, 0.50f, 1f);
    static readonly Color LOSE_COLOR = new Color(0.90f, 0.32f, 0.32f, 1f);

    [Header("씬에 미리 배치된 자식 오브젝트 연결")]
    [SerializeField] TextMeshProUGUI _resultText;
    [SerializeField] Button          _lobbyBtn;

    bool _returning; // 중복 클릭으로 REQ_GAME_RETURNCHAT을 여러 번 보내는 것 방지

    void Awake()
    {
        EnsureEventSystem();

        if (_resultText == null || _lobbyBtn == null)
        {
            Debug.LogError("[GameResultUI] _resultText / _lobbyBtn이 Inspector에 연결되어 있지 않습니다.");
            return;
        }

        _lobbyBtn.onClick.AddListener(OnLobbyButtonClicked);

        if (Client.Instance != null)
            Client.Instance.OnReturnChatResponse += OnReturnChatResponse;
    }

    void OnDestroy()
    {
        if (Client.Instance != null)
            Client.Instance.OnReturnChatResponse -= OnReturnChatResponse;
    }

    /// <summary>이 씬에 EventSystem이 없으면(GameScene은 지금까지 없었음) 하나 만든다 — 없으면 버튼 클릭 자체가 안 먹는다.</summary>
    static void EnsureEventSystem()
    {
        if (EventSystem.current != null) return;

        new GameObject("EventSystem", typeof(EventSystem), typeof(InputSystemUIInputModule));
    }

    /// <summary>승패 결과를 띄운다. isWin=true면 승리, false면 패배.</summary>
    public void Show(bool isWin)
    {
        // 씬에 비활성 상태로 배치된 오브젝트라 Awake가 여기서 처음 실행된다(Unity 표준 동작:
        // 시작부터 비활성인 오브젝트는 SetActive(true)가 호출되기 전까지 Awake가 지연된다).
        gameObject.SetActive(true);

        _resultText.text  = isWin ? "승리!" : "패배...";
        _resultText.color = isWin ? WIN_COLOR : LOSE_COLOR;
    }

    void OnLobbyButtonClicked()
    {
        if (_returning) return;
        _returning = true;
        if (_lobbyBtn != null) _lobbyBtn.interactable = false;

        Client.Instance?.SendReturnChatRequest();
    }

    /// <summary>Client.OnReturnChatResponse 콜백. RES_GAME_RETURNCHAT 도착 후에야 로비 씬으로 전환한다.</summary>
    void OnReturnChatResponse(bool success)
    {
        SceneManager.LoadScene(LOBBY_SCENE_NAME);
    }
}
