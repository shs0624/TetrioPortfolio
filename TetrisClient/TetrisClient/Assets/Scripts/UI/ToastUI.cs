using System.Collections;
using UnityEngine;
using UnityEngine.UI;
using TMPro;

/// <summary>
/// 메시지를 잠깐 보여주는 토스트 알림 컴포넌트.
///
/// ── Inspector 연결 ───────────────────────────────────────────────
///  toastPanel          : 토스트 루트 GameObject (기본 비활성)
///  toastText           : 메시지 TMP_Text
///  closeBtn            : X 닫기 버튼 (선택)
///  autoDismissDuration : 기본 자동 닫힘 시간(초). 0 이하 = 자동 닫힘 없음.
///
/// ── 사용법 ──────────────────────────────────────────────────────
///  toast.Show("메시지");        // Inspector의 autoDismissDuration 후 자동 닫힘
///  toast.Show("메시지", 5f);   // 5초 후 자동 닫힘
///  toast.Show("메시지", 0f);   // 자동 닫힘 없음 (X 버튼 / Hide() 로만 닫힘)
///  toast.Hide();               // 즉시 닫기
/// </summary>
public class ToastUI : MonoBehaviour
{
    // ── Inspector ─────────────────────────────────────────────────
    [Header("References")]
    [SerializeField] GameObject toastPanel;
    [SerializeField] TMP_Text   toastText;
    [SerializeField] Button     closeBtn;

    [Header("Settings")]
    [Tooltip("기본 자동 닫힘 시간(초). 0 이하이면 자동 닫힘 없음.")]
    [SerializeField] float autoDismissDuration = 3f;

    // ── 내부 상태 ─────────────────────────────────────────────────
    Coroutine _autoDismissCoroutine;

    // ══════════════════════════════════════════════════════════════
    // Unity Lifecycle
    // ══════════════════════════════════════════════════════════════

    void Awake()
    {
        if (toastPanel != null) toastPanel.SetActive(false);
        if (closeBtn   != null) closeBtn.onClick.AddListener(Hide);
    }

    // ══════════════════════════════════════════════════════════════
    // Public API
    // ══════════════════════════════════════════════════════════════

    /// <summary>
    /// 토스트를 표시합니다. 이미 표시 중이면 메시지와 타이머를 갱신합니다.
    /// </summary>
    /// <param name="message">표시할 메시지</param>
    /// <param name="duration">
    /// 자동 닫힘 시간(초).
    /// 생략 시 Inspector 설정값(autoDismissDuration) 사용.
    /// 0 이하이면 자동 닫힘 없음 — X 버튼 또는 Hide() 로만 닫힘.
    /// </param>
    public void Show(string message, float duration = -1f)
    {
        if (toastText  != null) toastText.text = message;
        if (toastPanel != null) toastPanel.SetActive(true);

        // 진행 중인 자동 닫힘 취소 후 새 타이머 시작
        if (_autoDismissCoroutine != null)
        {
            StopCoroutine(_autoDismissCoroutine);
            _autoDismissCoroutine = null;
        }

        float d = duration < 0f ? autoDismissDuration : duration;
        if (d > 0f)
            _autoDismissCoroutine = StartCoroutine(AutoDismissCoroutine(d));
    }

    /// <summary>
    /// 토스트를 즉시 닫습니다.
    /// </summary>
    public void Hide()
    {
        if (_autoDismissCoroutine != null)
        {
            StopCoroutine(_autoDismissCoroutine);
            _autoDismissCoroutine = null;
        }

        if (toastPanel != null) toastPanel.SetActive(false);
    }

    // ══════════════════════════════════════════════════════════════
    // 내부
    // ══════════════════════════════════════════════════════════════

    IEnumerator AutoDismissCoroutine(float duration)
    {
        yield return new WaitForSeconds(duration);
        if (toastPanel != null) toastPanel.SetActive(false);
        _autoDismissCoroutine = null;
    }
}
