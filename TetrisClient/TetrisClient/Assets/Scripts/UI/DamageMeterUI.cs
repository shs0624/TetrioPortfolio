using UnityEngine;

/// <summary>
/// 보드 왼쪽에 세로로 쌓이는 회색 데미지(대기 가비지) 미터. Tetr.io의 인커밍 가비지 바를 참고했다.
/// 서버 en_PACKET_CS_TETRIS_ACK_GAME_DAMAGE의 DamageCount를 그대로 반영만 한다(예측하지 않음).
/// Board.GridToWorld를 그대로 재사용해 보드와 같은 좌표계로 정렬하므로 별도 UI 캔버스가 필요 없다.
/// </summary>
public class DamageMeterUI : MonoBehaviour
{
    const int   METER_COL = -1; // 보드 왼쪽 한 칸
    static readonly Color SEGMENT_COLOR = new Color(0.4f, 0.4f, 0.4f, 1f); // 기존 가비지 블록 틴트와 동일

    Board  _board;
    Sprite[] _blockSprites;
    Material _blockMaterial;

    readonly System.Collections.Generic.List<Transform> _segments = new System.Collections.Generic.List<Transform>();

    /// <summary>보드/스프라이트 참조를 넘겨 초기화한다. GameManager.Start()에서 한 번 호출.</summary>
    public void Init(Board board, Sprite[] blockSprites, Material blockMaterial)
    {
        _board         = board;
        _blockSprites  = blockSprites;
        _blockMaterial = blockMaterial;
    }

    /// <summary>대기 데미지 수만큼 아래에서 위로 회색 칸을 쌓아 다시 그린다. 0이면 전부 비운다.</summary>
    public void SetCount(int count)
    {
        ClearSegments();
        if (_board == null) return;

        count = Mathf.Clamp(count, 0, Board.Height);

        for (int row = 0; row < count; row++)
        {
            var go = new GameObject("Damage_" + row);
            go.transform.SetParent(transform, false);
            go.transform.position = _board.GridToWorld(METER_COL, row);

            var sr = go.AddComponent<SpriteRenderer>();
            sr.sprite         = _blockSprites[0];
            sr.sharedMaterial = _blockMaterial;
            sr.color          = SEGMENT_COLOR;
            sr.sortingOrder   = 2;

            _segments.Add(go.transform);
        }
    }

    void ClearSegments()
    {
        for (int i = 0; i < _segments.Count; i++)
        {
            if (_segments[i] != null) Destroy(_segments[i].gameObject);
        }
        _segments.Clear();
    }
}
