using System;
using UnityEngine;
using UnityEngine.UI;
using TMPro;

/// <summary>
/// 화면 전체를 덮는 범용 확인(OK) 모달. 메시지와 확인 버튼만 있고,
/// 확인을 누르면 지정한 콜백을 실행한 뒤 스스로 닫힌다.
/// CountdownUI와 마찬가지로 Canvas를 포함한 UI 전체를 런타임에 코드로 생성하므로
/// 씬에 별도로 배치하거나 인스펙터로 연결할 필요가 없다.
/// </summary>
public class ConfirmDialogUI : MonoBehaviour
{
    const string FONT_PATH = "Fonts & Materials/LiberationSans SDF";

    GameObject      _root;
    TextMeshProUGUI _messageText;
    Action          _onConfirm;

    void Awake()
    {
        BuildUI();
        _root.SetActive(false);
    }

    void BuildUI()
    {
        var canvasGO = new GameObject("ConfirmDialogCanvas", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
        canvasGO.transform.SetParent(transform, false);
        _root = canvasGO;

        var canvas = canvasGO.GetComponent<Canvas>();
        canvas.renderMode   = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 2000; // 다른 UI보다 항상 위에

        var scaler = canvasGO.GetComponent<CanvasScaler>();
        scaler.uiScaleMode         = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920, 1080);

        // ── 반투명 마스킹 ──────────────────────────────────────────
        var maskGO = new GameObject("Mask", typeof(Image));
        maskGO.transform.SetParent(canvasGO.transform, false);
        var maskRt = maskGO.GetComponent<RectTransform>();
        maskRt.anchorMin = Vector2.zero;
        maskRt.anchorMax = Vector2.one;
        maskRt.offsetMin = Vector2.zero;
        maskRt.offsetMax = Vector2.zero;
        maskGO.GetComponent<Image>().color = new Color(0f, 0f, 0f, 0.65f);

        // ── 패널 ──────────────────────────────────────────────────
        var panelGO = new GameObject("Panel", typeof(Image));
        panelGO.transform.SetParent(canvasGO.transform, false);
        var panelRt = panelGO.GetComponent<RectTransform>();
        panelRt.anchorMin = new Vector2(0.5f, 0.5f);
        panelRt.anchorMax = new Vector2(0.5f, 0.5f);
        panelRt.sizeDelta = new Vector2(720, 320);
        panelGO.GetComponent<Image>().color = new Color(0.12f, 0.12f, 0.16f, 1f);

        var font = Resources.Load<TMP_FontAsset>(FONT_PATH);

        // ── 메시지 텍스트 ──────────────────────────────────────────
        var textGO = new GameObject("Message", typeof(TextMeshProUGUI));
        textGO.transform.SetParent(panelGO.transform, false);
        var textRt = textGO.GetComponent<RectTransform>();
        textRt.anchorMin = new Vector2(0f, 0.35f);
        textRt.anchorMax = new Vector2(1f, 1f);
        textRt.offsetMin = new Vector2(40, 0);
        textRt.offsetMax = new Vector2(-40, -30);

        _messageText = textGO.GetComponent<TextMeshProUGUI>();
        _messageText.font              = font;
        _messageText.alignment         = TextAlignmentOptions.Center;
        _messageText.fontSize          = 36f;
        _messageText.color             = Color.white;
        _messageText.enableWordWrapping = true;
        _messageText.raycastTarget     = false;

        // ── 확인 버튼 ──────────────────────────────────────────────
        var btnGO = new GameObject("ConfirmBtn", typeof(Image), typeof(Button));
        btnGO.transform.SetParent(panelGO.transform, false);
        var btnRt = btnGO.GetComponent<RectTransform>();
        btnRt.anchorMin        = new Vector2(0.5f, 0f);
        btnRt.anchorMax        = new Vector2(0.5f, 0f);
        btnRt.anchoredPosition = new Vector2(0, 50);
        btnRt.sizeDelta        = new Vector2(220, 64);
        btnGO.GetComponent<Image>().color = new Color(0.20f, 0.32f, 0.88f, 1f);

        var btnTextGO = new GameObject("Text", typeof(TextMeshProUGUI));
        btnTextGO.transform.SetParent(btnGO.transform, false);
        var btnTextRt = btnTextGO.GetComponent<RectTransform>();
        btnTextRt.anchorMin = Vector2.zero;
        btnTextRt.anchorMax = Vector2.one;
        btnTextRt.offsetMin = Vector2.zero;
        btnTextRt.offsetMax = Vector2.zero;

        var btnText = btnTextGO.GetComponent<TextMeshProUGUI>();
        btnText.text           = "확인";
        btnText.font            = font;
        btnText.alignment       = TextAlignmentOptions.Center;
        btnText.fontSize        = 28f;
        btnText.color           = Color.white;
        btnText.raycastTarget   = false;

        btnGO.GetComponent<Button>().onClick.AddListener(OnConfirmClicked);
    }

    /// <summary>메시지를 띄운다. 확인 버튼을 누르면 onConfirm을 실행한 뒤 스스로 닫힌다.</summary>
    public void Show(string message, Action onConfirm)
    {
        _messageText.text = message;
        _onConfirm         = onConfirm;
        _root.SetActive(true);
    }

    void OnConfirmClicked()
    {
        _root.SetActive(false);
        var cb = _onConfirm;
        _onConfirm = null;
        cb?.Invoke();
    }
}
