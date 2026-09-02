using System;
using System.Collections;
using UnityEngine;
using UnityEngine.UI;
using TMPro;

/// <summary>
/// 게임 시작 카운트다운 연출. 화면 전체에 반투명 마스킹을 깔고, 가운데에 숫자를
/// 하나씩 페이드 인 → 유지 → 페이드 아웃으로 보여준 뒤 "START!"를 짧게 띄우고
/// 마스킹까지 함께 페이드 아웃한다. 서버 ACK_COUNTDOWN(en_PACKET_SC_TETRIS_ACK_COUNTDOWN)
/// 수신 시 GameManager가 PlayCountdown()을 호출해 트리거한다.
/// 전체 UI(Canvas 포함)는 런타임에 코드로 생성하므로 씬에 별도로 배치할 필요가 없다.
/// </summary>
public class CountdownUI : MonoBehaviour
{
    const string FONT_PATH = "Fonts & Materials/LiberationSans SDF";

    const float NUMBER_FADE_IN  = 0.15f;
    const float NUMBER_HOLD     = 0.55f;
    const float NUMBER_FADE_OUT = 0.30f;
    const float START_FADE_IN   = 0.15f;
    const float START_HOLD      = 0.35f;
    const float MASK_FADE_OUT   = 0.30f;

    const float MASK_ALPHA = 0.65f;

    CanvasGroup     _maskGroup;
    CanvasGroup     _textGroup;
    TextMeshProUGUI _text;

    void Awake()
    {
        BuildUI();
    }

    void BuildUI()
    {
        var canvasGO = new GameObject("CountdownCanvas", typeof(Canvas), typeof(CanvasScaler));
        canvasGO.transform.SetParent(transform, false);

        var canvas = canvasGO.GetComponent<Canvas>();
        canvas.renderMode   = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1000; // 다른 UI보다 항상 위에

        var scaler = canvasGO.GetComponent<CanvasScaler>();
        scaler.uiScaleMode         = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920, 1080);

        // ── 반투명 마스킹 ──────────────────────────────────────────
        var maskGO = new GameObject("Mask", typeof(Image), typeof(CanvasGroup));
        maskGO.transform.SetParent(canvasGO.transform, false);

        var maskRt = maskGO.GetComponent<RectTransform>();
        maskRt.anchorMin = Vector2.zero;
        maskRt.anchorMax = Vector2.one;
        maskRt.offsetMin = Vector2.zero;
        maskRt.offsetMax = Vector2.zero;

        var maskImg = maskGO.GetComponent<Image>();
        maskImg.color         = Color.black; // 실제 밝기는 CanvasGroup.alpha로 제어
        maskImg.raycastTarget = false;

        _maskGroup = maskGO.GetComponent<CanvasGroup>();
        _maskGroup.alpha          = 0f;
        _maskGroup.blocksRaycasts = false;

        // ── 중앙 숫자 / START! 텍스트 ──────────────────────────────
        var textGO = new GameObject("CountdownText", typeof(TextMeshProUGUI), typeof(CanvasGroup));
        textGO.transform.SetParent(canvasGO.transform, false);

        var textRt = textGO.GetComponent<RectTransform>();
        textRt.anchorMin        = new Vector2(0.5f, 0.5f);
        textRt.anchorMax        = new Vector2(0.5f, 0.5f);
        textRt.sizeDelta        = new Vector2(900, 400);
        textRt.anchoredPosition = Vector2.zero;

        _text                = textGO.GetComponent<TextMeshProUGUI>();
        _text.font           = Resources.Load<TMP_FontAsset>(FONT_PATH);
        _text.alignment      = TextAlignmentOptions.Center;
        _text.fontSize       = 160f;
        _text.fontStyle      = FontStyles.Bold;
        _text.color          = Color.white;
        _text.raycastTarget  = false;

        _textGroup = textGO.GetComponent<CanvasGroup>();
        _textGroup.alpha = 0f;
    }

    /// <summary>
    /// seconds초부터 1초 간격으로 숫자를 페이드 인/아웃하며 표시한 뒤 "START!"를 짧게 띄우고
    /// 마스킹을 걷어낸다. 완료되면 onComplete가 호출된다 (예: 입력/게임 로직 재개).
    /// </summary>
    public void PlayCountdown(int seconds, Action onComplete = null)
    {
        StopAllCoroutines();
        StartCoroutine(CoPlayCountdown(seconds, onComplete));
    }

    IEnumerator CoPlayCountdown(int seconds, Action onComplete)
    {
        yield return CoFade(_maskGroup, 0f, MASK_ALPHA, NUMBER_FADE_IN);

        for (int n = Mathf.Max(seconds, 1); n >= 1; n--)
        {
            _text.text       = n.ToString();
            _textGroup.alpha = 0f;
            yield return CoFade(_textGroup, 0f, 1f, NUMBER_FADE_IN);
            yield return new WaitForSeconds(NUMBER_HOLD);
            yield return CoFade(_textGroup, 1f, 0f, NUMBER_FADE_OUT);
        }

        _text.text       = "START!";
        _textGroup.alpha = 0f;
        yield return CoFade(_textGroup, 0f, 1f, START_FADE_IN);
        yield return new WaitForSeconds(START_HOLD);

        // START! 텍스트와 마스킹을 함께 걷어낸다.
        yield return CoFadeBoth(MASK_FADE_OUT);

        onComplete?.Invoke();
    }

    IEnumerator CoFade(CanvasGroup group, float from, float to, float duration)
    {
        if (duration <= 0f) { group.alpha = to; yield break; }

        float t = 0f;
        while (t < duration)
        {
            t += Time.deltaTime;
            group.alpha = Mathf.Lerp(from, to, Mathf.Clamp01(t / duration));
            yield return null;
        }
        group.alpha = to;
    }

    IEnumerator CoFadeBoth(float duration)
    {
        float t = 0f;
        float textFrom = _textGroup.alpha;
        float maskFrom = _maskGroup.alpha;
        while (t < duration)
        {
            t += Time.deltaTime;
            float k = Mathf.Clamp01(t / duration);
            _textGroup.alpha = Mathf.Lerp(textFrom, 0f, k);
            _maskGroup.alpha = Mathf.Lerp(maskFrom, 0f, k);
            yield return null;
        }
        _textGroup.alpha = 0f;
        _maskGroup.alpha = 0f;
    }
}
