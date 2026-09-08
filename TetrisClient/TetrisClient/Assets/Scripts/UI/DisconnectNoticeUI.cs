using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;

/// <summary>
/// 서버가 세션을 강제로 끊었을 때(Disconnect) 로비/게임 어디에 있든 공통으로
/// "연결이 끊겼습니다!" 안내를 화면 전체 마스크와 함께 띄우고, 확인을 누르면
/// 로그인 씬으로 돌려보낸다.
///
/// Client는 DontDestroyOnLoad 싱글톤이라 씬을 넘나들며 계속 살아있고
/// OnDisconnected 이벤트도 이미 정상적으로 발생하지만(소켓 정리/State=Idle까지
/// 끝난 뒤 발생), LobbyUI/GameManager/GameResultUI 중 누구도 이 이벤트를
/// 구독하지 않아서 로비·게임 도중 서버가 끊으면 화면엔 아무 반응이 없었다.
///
/// 이 스크립트는 씬에 배치할 필요 없이, 게임 시작 시 딱 한 번 스스로
/// DontDestroyOnLoad 오브젝트를 만들어 모든 씬에서 이 이벤트를 대신 감시한다.
/// 마스크+메시지+확인 버튼 UI는 새로 만들지 않고 기존 ConfirmDialogUI를 그대로
/// 재사용한다.
/// </summary>
public class DisconnectNoticeUI : MonoBehaviour
{
    ConfirmDialogUI _confirmDialog;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Bootstrap()
    {
        var go = new GameObject("DisconnectNotice");
        go.AddComponent<DisconnectNoticeUI>();
        DontDestroyOnLoad(go);
    }

    void Awake()
    {
        _confirmDialog = gameObject.AddComponent<ConfirmDialogUI>();

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

        _confirmDialog.Show("연결이 끊겼습니다!", () =>
        {
            Client.Instance?.Disconnect();
            SceneManager.LoadScene("LoginScene");
        });
    }

    /// <summary>GameScene은 씬 자체에 EventSystem이 없어서(GameResultUI.cs와 동일한 이유), 없으면 하나 만든다.</summary>
    static void EnsureEventSystem()
    {
        if (EventSystem.current != null) return;

        new GameObject("EventSystem", typeof(EventSystem), typeof(InputSystemUIInputModule));
    }
}
