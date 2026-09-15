using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Collections.Concurrent;
using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// 온라인 TCP 클라이언트. 버튼이 눌릴 때마다 아래 단계가 개별적으로 진행됩니다.
///
///   0) ConnectToLoginServer()  — 게임 시작 시 1회, 로그인 서버에 접속만 해둔다.
///   1) CheckId() / CheckNickname() / Register()  — 회원가입 팝업의 각 버튼.
///   2) Login()  — 로그인 버튼.
///        ├─ 로그인 서버에 REQ_LOGIN 전송 → 응답 성공 시 로그인 서버 연결을 끊고
///        └─ 응답에 담긴 게임서버 IP:Port로 재접속 → 게임서버에 REQ_LOGIN(AccountNum+SessionKey) 전송
///           → 게임서버 RES_LOGIN까지 받아야 최종 로그인 성공 콜백이 호출된다.
///
/// 로그인 실패(아이디 없음/비번 틀림)는 서버가 스스로 연결을 끊는 설계이므로,
/// 클라이언트는 실패 콜백을 받은 뒤 재로그인 시 ConnectToLoginServer()부터 다시 호출해야 한다.
///
/// ── 와이어 포맷 (서버 CSerializationBuffer.h / NetServer.cpp 기준) ──────────────
///   [FixedKey(1, cleartext)][shLen(2, LE, payload 길이만)][RandKey(1, cleartext)]
///   [CheckSum(1, 암호화됨)][payload : WORD PacketID(2) + body...]  (payload도 암호화됨)
///
///   암호화는 CheckSum 바이트부터 payload 끝까지를 대상으로 하는 커스텀 스트림 암호이며,
///   서버 Encode()/Decode()와 100% 동일한 알고리즘을 아래 EncodeInPlace/DecodeInPlace에 재구현했다.
///   키(K)는 서버의 _FixedKey(=0x32)와 동일해야 하고, 헤더의 FixedKey 필드 값 자체는
///   서버 관례를 따라 _ProgramKey(=0x77)를 채워 보낸다(수신측에서 검증하지는 않는 것으로 보임).
/// </summary>
public class Client : MonoBehaviour
{
    // ── Singleton ─────────────────────────────────────────────────────
    public static Client Instance { get; private set; }

    // ── Inspector ─────────────────────────────────────────────────────
    [Header("Login Server")]
    public string loginIP   = "127.0.0.1";
    public int    loginPort = 7777;

    // ── State ─────────────────────────────────────────────────────────
    public enum NetState { Idle, LoginConnecting, LoginReady, GameConnecting, GameReady }
    public NetState State { get; private set; } = NetState.Idle;
    public bool IsConnected => _socket != null && _socket.Connected;
    /// <summary>로그인 서버에서 로그인 성공 시 받은 내 AccountNum. 로비 유저 목록에서 "나"를 구분하는 데 쓴다.</summary>
    public long MyAccountNum => _accountNum;
    /// <summary>내 닉네임. 게임서버 로그인 직후 서버가 보내는 자기 자신의 ACK_CHAT_ENTER에서 기억해둔다.</summary>
    public string MyNickname { get; private set; } = "";

    // ── Wire format constants (서버 Protocol.h / NetServer.h 와 반드시 일치해야 함) ──
    const byte FIXED_KEY   = 0x32;  // 서버 _FixedKey  (암/복호화 키)
    const byte PROGRAM_KEY = 0x77;  // 서버 _ProgramKey (헤더 FixedKey 필드에 실어 보내는 값)
    const int  HEADER_SIZE = 5;     // FixedKey(1) + shLen(2) + RandKey(1) + CheckSum(1)
    const int  PROTOCOL_MAX_SIZE = 500;

    const int ID_FIELD_LEN            = 20;  // char ID[20]
    const int PW_FIELD_LEN            = 20;  // char Passwd[20]
    const int NICK_FIELD_LEN          = 20;  // char Nickname[20]
    const int GAME_SESSIONKEY_CHARS   = 64;  // 게임서버가 읽는 WCHAR SessionKey[64] (문자 수)
    const int LOGIN_SESSIONKEY_CHARS  = 64;  // 로그인서버가 보내는 WCHAR SessionKey[64] (문자 수)
    const int GAME_IP_CHARS           = 16;  // WCHAR GameIP[16] (문자 수)

    // ── Socket & Buffers ──────────────────────────────────────────────
    Socket _socket;
    readonly byte[] _recvBuf    = new byte[1024 * 8];
    readonly byte[] _pendingBuf = new byte[1024 * 16];
    int _pendingSize;

    // ── Send queue (non-blocking) ─────────────────────────────────────
    readonly ConcurrentQueue<byte[]> _sendQueue = new ConcurrentQueue<byte[]>();
    bool _isSending;
    readonly object _sendLock = new object();

    // ── Main-thread dispatcher ────────────────────────────────────────
    readonly ConcurrentQueue<Action> _mainQueue = new ConcurrentQueue<Action>();

    // ── 로그인 성공 후 게임서버 접속에 필요한 정보 ─────────────────────
    long   _accountNum;
    string _gameIP;
    ushort _gamePort;
    string _sessionKey;

    // ── 진행 중인 요청의 콜백 (한 번에 하나씩만 진행한다고 가정) ────────
    enum DupCheckKind { None, Id, Nickname }
    DupCheckKind      _pendingDupCheckKind;
    Action<bool>      _pendingDupCheckCb;
    Action<bool>      _pendingRegisterCb;
    Action<bool, string> _pendingLoginCb;

    // ── Events ────────────────────────────────────────────────────────
    /// <summary>게임 서버 로그인까지 최종 성공했을 때</summary>
    public event Action OnGameServerConnected;
    /// <summary>소켓 오류 또는 서버 강제 종료 (사유 문자열)</summary>
    public event Action<string> OnDisconnected;

    /// <summary>로비 채팅방 입장 알림 (accountNum, nickname)</summary>
    public event Action<long, string> OnChatUserEnter;
    /// <summary>로비 채팅방 퇴장 알림 (accountNum, nickname)</summary>
    public event Action<long, string> OnChatUserExit;
    /// <summary>로비 채팅 메시지 수신 (accountNum, nickname, message)</summary>
    public event Action<long, string, string> OnChatMessage;
    /// <summary>게임 시작 준비 완료(RES_GAME_READY) 응답 수신 (success)</summary>
    public event Action<bool> OnGameReadyResponse;
    /// <summary>로비 복귀 요청 응답(RES_GAME_RETURNCHAT) 수신 (success) — 이 통지 후 로비 씬으로 전환한다.</summary>
    public event Action<bool> OnReturnChatResponse;
    /// <summary>카운트다운 시작 통지 (seconds) — 이후 진행은 클라이언트가 독자적으로 연출한다.</summary>
    public event Action<int> OnCountdown;
    /// <summary>보드 스냅샷 수신 (holdingBlock, nextBag[5], myBoard[200], opponentBoard[200] — 보드 둘 다 row-major, row0=서버 상단)</summary>
    public event Action<TetBlockType, TetBlockType[], byte[], byte[]> OnBoardUpdate;
    /// <summary>현재 낙하 중인 블록 갱신 (isSelf, blockType, rotate, x, y) — x/y는 signed(음수 원점 가능).
    /// isSelf가 false면 상대방의 낙하 블록 정보(서버 필드명은 IsOpponent지만 실제론 반대 의미 — 1이면 자기 자신 블록).</summary>
    public event Action<bool, TetBlockType, byte, sbyte, sbyte> OnBlockUpdate;
    /// <summary>대기 중인 가비지(데미지) 줄 수 갱신 (본인 기준)</summary>
    public event Action<int> OnDamageUpdate;
    /// <summary>게임 결과 통지 (isWin) — true면 승리, false면 패배</summary>
    public event Action<bool> OnGameResult;
    /// <summary>매칭 요청 응답 (success) — true면 매칭 대기열에 들어간 것뿐, 상대방 매칭 완료는 별도 통지.</summary>
    public event Action<bool> OnMatchResponse;
    /// <summary>매칭 취소 요청 응답(RES_MATCHING_CANCEL) 수신 (success)</summary>
    public event Action<bool> OnMatchCancelResponse;
    /// <summary>매칭 성공(상대방 확정) 통지 (opAccountNum, opNickname)</summary>
    public event Action<long, string> OnMatchSuccess;

    /// <summary>
    /// 현재 로비 채팅방에 입장해 있는 유저 스냅샷 (accountNum -> nickname).
    /// LobbyUI가 Start() 시점에 구독하기 전에 이미 도착한 ACK_CHAT_ENTER(특히 자기 자신 입장)가
    /// 유실되는 레이스를 막기 위해, 서버가 ACK_CHAT_ENTER/EXIT를 보낼 때마다 이 딥셔너리를 갱신해둔다.
    /// </summary>
    readonly Dictionary<long, string> _chatRoster = new Dictionary<long, string>();

    /// <summary>현재 채팅방 유저 스냅샷 (accountNum -> nickname). LobbyUI가 Start() 시점에 이것을 먼저 읽어 초기 목록을 채운다.</summary>
    public IReadOnlyDictionary<long, string> ChatRoster => _chatRoster;

    // ── Unity lifecycle ───────────────────────────────────────────────
    void Awake()
    {
        if (Instance != null && Instance != this) { Destroy(gameObject); return; }
        Instance = this;
        DontDestroyOnLoad(gameObject);
    }


    // 하트비트 설정
    const float HEARTBEAT_INTERVAL = 30f;
    Coroutine   _heartbeatCoroutine;

    void Update()
    {
        // 비동기 콜백에서 유니티 메인 스레드로 디스패치
        while (_mainQueue.TryDequeue(out var action)) action();
    }

    /// <summary>
    /// 게임 서버 연결 성공 시 시작되는 코루틴.
    /// 30초마다 TETRIS_REQ_HEARTBEAT 패킷을 전송합니다.
    /// </summary>
    System.Collections.IEnumerator HeartbeatCoroutine(PacketID packetID, NetState targetState)
    {
        var wait = new UnityEngine.WaitForSeconds(HEARTBEAT_INTERVAL);
        while (true)
        {
            yield return wait;
            // 대상 상태가 아니거나 소켓이 끊키면 코루틴 종료
            if (State != targetState || !IsConnected) yield break;
            SendToServer((ushort)packetID, System.Array.Empty<byte>());
            Debug.Log($"[Client] Heartbeat sent ({packetID}).");
        }
    }

    // 게임 서버용 하트비트 (GameReady 상태)
    void StartHeartbeat()
    {
        StopHeartbeat();
        _heartbeatCoroutine = StartCoroutine(
            HeartbeatCoroutine(PacketID.TETRIS_REQ_HEARTBEAT, NetState.GameReady));
    }

    // 로그인 서버용 하트비트 (LoginReady 상태)
    void StartLoginHeartbeat()
    {
        StopHeartbeat();
        _heartbeatCoroutine = StartCoroutine(
            HeartbeatCoroutine(PacketID.TETRISLOGIN_REQ_HEARTBEAT, NetState.LoginReady));
    }

    void StopHeartbeat()
    {
        if (_heartbeatCoroutine != null)
        {
            StopCoroutine(_heartbeatCoroutine);
            _heartbeatCoroutine = null;
        }
    }

                void OnApplicationQuit() => Disconnect();
    void OnDestroy()          => Disconnect();

    // ════════════════════════════════════════════════════════════════════
    // STEP 0 : 로그인 서버 접속 (게임 시작 시 1회 호출)
    // ════════════════════════════════════════════════════════════════════

    /// <summary>로그인 서버에 접속만 해둔다. 이후 CheckId/CheckNickname/Register/Login이 이 연결을 재사용한다.</summary>
    public void ConnectToLoginServer(Action onConnected = null, Action<string> onError = null)
    {
        if (State != NetState.Idle)
        {
            Debug.LogWarning("[Client] Already connecting or connected.");
            return;
        }

        State = NetState.LoginConnecting;
        _socket = NewSocket();
        Debug.Log($"[Client] Connecting to login server {loginIP}:{loginPort}...");

        try
        {
            var ep = new IPEndPoint(IPAddress.Parse(loginIP), loginPort);
            _socket.BeginConnect(ep, ar =>
            {
                try
                {
                    _socket.EndConnect(ar);
                    State = NetState.LoginReady;
                // TODO: 서버 Protocol.h에 TETRISLOGIN_REQ_HEARTBEAT(19) 추가 후 아래 주석 해제
                    _mainQueue.Enqueue(StartLoginHeartbeat);
                    StartReceive();
                    Debug.Log("[Client] Login server connected.");
                    _mainQueue.Enqueue(() => onConnected?.Invoke());
                }
                catch (Exception e)
                {
                    State = NetState.Idle;
                    _mainQueue.Enqueue(() => onError?.Invoke(e.Message));
                }
            }, null);
        }
        catch (Exception e)
        {
            State = NetState.Idle;
            onError?.Invoke(e.Message);
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // STEP 1 : 아이디/닉네임 중복체크, 회원가입 (회원가입 팝업 버튼들)
    // ════════════════════════════════════════════════════════════════════

    /// <summary>아이디 중복체크 버튼. nickname은 팝업의 닉네임 입력칸 값을 그대로 넘기면 된다(서버가 두 필드를 항상 같이 읽음).</summary>
    public void CheckId(string id, string nickname, Action<bool> onResult)
    {
        if (!RequireLoginReady(onResult)) return;
        if (_pendingDupCheckKind != DupCheckKind.None)
        {
            Debug.LogWarning("[Client] Dup-check already in progress.");
            return;
        }

        _pendingDupCheckKind = DupCheckKind.Id;
        _pendingDupCheckCb   = onResult;
        SendDupCheck(PacketID.TETRISLOGIN_REQ_DUPCHECK_ID, id, nickname);
    }

    /// <summary>닉네임 중복체크 버튼. id는 팝업의 아이디 입력칸 값을 그대로 넘기면 된다.</summary>
    public void CheckNickname(string id, string nickname, Action<bool> onResult)
    {
        if (!RequireLoginReady(onResult)) return;
        if (_pendingDupCheckKind != DupCheckKind.None)
        {
            Debug.LogWarning("[Client] Dup-check already in progress.");
            return;
        }

        _pendingDupCheckKind = DupCheckKind.Nickname;
        _pendingDupCheckCb   = onResult;
        SendDupCheck(PacketID.TETRISLOGIN_REQ_DUPCHECK_NICKNAME, id, nickname);
    }

    void SendDupCheck(PacketID type, string id, string nickname)
    {
        var body = new byte[ID_FIELD_LEN + NICK_FIELD_LEN];
        WriteFixedAscii(body, 0,             id,       ID_FIELD_LEN);
        WriteFixedAscii(body, ID_FIELD_LEN,  nickname, NICK_FIELD_LEN);
        SendToServer((ushort)type, body);
    }

    /// <summary>회원가입 확인 버튼.</summary>
    public void Register(string id, string pw, string nickname, Action<bool> onResult)
    {
        if (!RequireLoginReady(onResult)) return;
        if (_pendingRegisterCb != null)
        {
            Debug.LogWarning("[Client] Register already in progress.");
            return;
        }

        _pendingRegisterCb = onResult;

        var body = new byte[ID_FIELD_LEN + PW_FIELD_LEN + NICK_FIELD_LEN];
        WriteFixedAscii(body, 0,                             id,       ID_FIELD_LEN);
        WriteFixedAscii(body, ID_FIELD_LEN,                  pw,       PW_FIELD_LEN);
        WriteFixedAscii(body, ID_FIELD_LEN + PW_FIELD_LEN,   nickname, NICK_FIELD_LEN);
        SendToServer((ushort)PacketID.TETRISLOGIN_REQ_REGISTER, body);
    }

    // ════════════════════════════════════════════════════════════════════
    // STEP 2 : 로그인 버튼 — 로그인 서버 인증 → 게임 서버 재접속 → 게임 서버 로그인
    // ════════════════════════════════════════════════════════════════════

    /// <summary>
    /// 로그인 버튼. 최종 콜백은 게임 서버 로그인까지 성공해야 (true, null)로 호출된다.
    /// 실패 시 (false, 사유)로 호출되며, 그 시점엔 소켓이 이미 정리된 상태다(재로그인하려면 ConnectToLoginServer부터 다시).
    /// </summary>
    public void Login(string id, string pw, Action<bool, string> onResult)
    {
        if (!RequireLoginReady(ok => onResult?.Invoke(false, "로그인 서버에 연결되어 있지 않습니다."))) return;
        if (_pendingLoginCb != null)
        {
            Debug.LogWarning("[Client] Login already in progress.");
            return;
        }

        _pendingLoginCb = onResult;

        var body = new byte[ID_FIELD_LEN + PW_FIELD_LEN];
        WriteFixedAscii(body, 0,            id, ID_FIELD_LEN);
        WriteFixedAscii(body, ID_FIELD_LEN, pw, PW_FIELD_LEN);
        SendToServer((ushort)PacketID.TETRISLOGIN_REQ_LOGIN, body);
    }

    // 로그인 서버의 전체 로그인 응답 처리.
    // ⚠️ 파일 상단 주석 참고: 서버가 실제로 이 응답에 TETRIS_RES_LOGIN(8) 태그를 사용함.
    void HandleLoginServerFullRes(byte[] body)
    {
        const int MIN_LEN = 1 + 8; // Status + AccountNum
        if (body.Length < MIN_LEN)
        {
            FailLogin("로그인 응답 파싱 실패 (payload too short)");
            return;
        }

        byte status     = body[0];
        long accountNum = BitConverter.ToInt64(body, 1);
        bool ok = (status == 1); // dfTETRIS_LOGIN_OK

        const int EXTRA_LEN = (GAME_IP_CHARS * 2) + 2 + (LOGIN_SESSIONKEY_CHARS * 2);
        if (!ok || body.Length < MIN_LEN + EXTRA_LEN)
        {
            FailLogin(LoginStatusToReason(status));
            return;
        }

        int offset = MIN_LEN;
        string gameIP = ReadFixedUtf16(body, offset, GAME_IP_CHARS);
        offset += GAME_IP_CHARS * 2;
        ushort gamePort = BitConverter.ToUInt16(body, offset);
        offset += 2;
        string sessionKey = ReadFixedUtf16(body, offset, LOGIN_SESSIONKEY_CHARS);

        _accountNum = accountNum;
        _gameIP     = gameIP;
        _gamePort   = gamePort;
        _sessionKey = sessionKey;

        Debug.Log($"[Client] Login server OK → Game server {gameIP}:{gamePort}");

        // 로그인 서버 연결을 끊고, 곧바로 게임 서버로 재접속한다. (_pendingLoginCb는 그대로 유지)
        // State를 여기서 바로 바꿔두지 않으면, 게임 서버 소켓으로 이미 갈아끼운 뒤에도
        // State가 한동안 LoginReady로 남아있어서 로그인 서버용 하트비트 코루틴이
        // (State == LoginReady && IsConnected 조건을 그대로 통과해) 게임 서버 소켓으로
        // TETRISLOGIN_REQ_HEARTBEAT를 잘못 흘려보낼 수 있다. StopHeartbeat()로 그 코루틴 자체도
        // 확실히 멈춰서 이중으로 막는다.
        StopHeartbeat();
        State = NetState.GameConnecting;
        CloseSocket();
        _pendingSize = 0;
        BeginConnectToGame(gameIP, gamePort);
    }

    void BeginConnectToGame(string ip, int port)
    {
        // 로그인 서버 소켓을 명시적으로 닫은 후 신규 소켓으로 교체한다.
        // Close하지 않으면 기존 BeginReceive 콜백이 FIN을 받아
        // DispatchError를 트리거하고 _pendingLoginCb가 소멸되어 씨 전환이 되지 않는다.
        var oldSocket = _socket;
        try { oldSocket?.Shutdown(SocketShutdown.Both); } catch { }
        try { oldSocket?.Close(); }                      catch { }
        _socket = NewSocket();
        Debug.Log($"[Client] Connecting to game server {ip}:{port}...");

        try
        {
            var ep = new IPEndPoint(IPAddress.Parse(ip), port);
            _socket.BeginConnect(ep, ar =>
            {
                try
                {
                    _socket.EndConnect(ar);
                    StartReceive();
                    Debug.Log("[Client] Game server connected. Sending TETRIS_REQ_LOGIN...");
                    _mainQueue.Enqueue(SendGameLoginReq);
                }
                catch (Exception e)
                {
                    State = NetState.Idle;
                    FailLogin("게임 서버 접속 실패: " + e.Message);
                }
            }, null);
        }
        catch (Exception e)
        {
            State = NetState.Idle;
            FailLogin("게임 서버 접속 예외: " + e.Message);
        }
    }

    void SendGameLoginReq()
    {
        var body = new byte[8 + GAME_SESSIONKEY_CHARS * 2];
        Buffer.BlockCopy(BitConverter.GetBytes(_accountNum), 0, body, 0, 8);
        WriteFixedUtf16(body, 8, _sessionKey, GAME_SESSIONKEY_CHARS);
        SendToServer((ushort)PacketID.TETRIS_REQ_LOGIN, body);
    }

    // 게임 서버의 단순 로그인 응답 처리.
    // ⚠️ 파일 상단 주석 참고: 서버가 실제로 이 응답에 TETRISLOGIN_RES_LOGIN(6) 태그를 사용함.
    void HandleGameServerSimpleRes(byte[] body)
    {
        byte status = body.Length > 0 ? body[0] : (byte)0;
        bool ok = (status == 1); // Win32 TRUE

        if (!ok)
        {
            State = NetState.Idle;
            FailLogin("게임 서버 로그인 실패");
            return;
        }

            State = NetState.GameReady;
            StartHeartbeat(); // 연결 성공 시 하트비트 코루틴 시작
        Debug.Log("[Client] Game server login OK!");

        var cb = _pendingLoginCb;
        _pendingLoginCb = null;
        cb?.Invoke(true, null);
        OnGameServerConnected?.Invoke();
    }

    void FailLogin(string reason)
    {
        Debug.LogWarning("[Client] Login failed: " + reason);
        var cb = _pendingLoginCb;
        _pendingLoginCb = null;
        CloseSocket();
        State = NetState.Idle;
        cb?.Invoke(false, reason);
    }

    static string LoginStatusToReason(byte status)
    {
        switch (status)
        {
            case 2: return "서버 상태 이상으로 로그인할 수 없습니다.";
            case 3: return "존재하지 않는 아이디입니다.";
            case 4: return "비밀번호가 일치하지 않습니다.";
            case 5: return "세션 키 오류가 발생했습니다.";
            default: return "로그인에 실패했습니다.";
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // STEP 3 : 로비 채팅 (채팅 씬)
    // ════════════════════════════════════════════════════════════════════

    /// <summary>
    /// 채팅 전송 버튼. 게임 서버 로그인 완료(GameReady) 상태에서만 보낼 수 있다.
    /// 서버가 최대 100자까지만 읽으므로(TetrisServer_Message.cpp _MaxMessageLen) 클라이언트에서도 잘라서 보낸다.
    /// </summary>
    public void SendChatMessage(string message)
    {
        if (State != NetState.GameReady || !IsConnected)
        {
            Debug.LogWarning("[Client] SendChatMessage called but not in GameReady state.");
            return;
        }
        if (string.IsNullOrEmpty(message)) return;

        const int MAX_MSG_CHARS = 100;
        if (message.Length > MAX_MSG_CHARS) message = message.Substring(0, MAX_MSG_CHARS);

        byte[] msgBytes = Encoding.Unicode.GetBytes(message);
        ushort msgLen   = (ushort)message.Length;

        var body = new byte[2 + msgBytes.Length];
        body[0] = (byte)(msgLen & 0xFF);
        body[1] = (byte)((msgLen >> 8) & 0xFF);
        Buffer.BlockCopy(msgBytes, 0, body, 2, msgBytes.Length);

        SendToServer((ushort)PacketID.TETRIS_REQ_CHAT_MESSAGE, body);
    }

    // S -> C 채팅방 입장 알림. body: INT64 AccountNum(8) + WCHAR Nickname[20](40)
    // S -> C 채팅방 입장 알림. body: INT64 AccountNum(8) + WCHAR Nickname[20](40)
    void HandleChatEnter(byte[] body)
    {
        const int LEN = 8 + 20 * 2;
        if (body.Length < LEN) { Debug.LogWarning("[Client] ChatEnter body too short."); return; }

        long   accountNum = BitConverter.ToInt64(body, 0);
        string nickname   = ReadFixedUtf16(body, 8, 20);
        _chatRoster[accountNum] = nickname;
        if (accountNum == _accountNum) MyNickname = nickname; // 자기 자신의 입장 알림 — 닉네임 기억
        OnChatUserEnter?.Invoke(accountNum, nickname);
    }

    // S -> C 채팅방 퇴장 알림. body: INT64 AccountNum(8) + WCHAR Nickname[20](40)
    // S -> C 채팅방 퇴장 알림. body: INT64 AccountNum(8) + WCHAR Nickname[20](40)
    void HandleChatExit(byte[] body)
    {
        const int LEN = 8 + 20 * 2;
        if (body.Length < LEN) { Debug.LogWarning("[Client] ChatExit body too short."); return; }

        long   accountNum = BitConverter.ToInt64(body, 0);
        string nickname   = ReadFixedUtf16(body, 8, 20);
        _chatRoster.Remove(accountNum);
        OnChatUserExit?.Invoke(accountNum, nickname);
    }

    // S -> C 채팅 메시지 브로드캐스트. body: INT64 AccountNum(8) + WCHAR Nickname[20](40) + WORD MessageLen(2) + WCHAR Message[MessageLen]
    // ⚠️ 서버 TetrisServer_Packet.cpp::mpRESChatMessage()가 현재 WORD PacketID를 안 써서 보내고 있어
    //    이 핸들러가 실제로 호출되려면 서버에 "(**cPacket) << (WORD)packetType;" 한 줄을 먼저 추가해야 한다.
    void HandleChatMessage(byte[] body)
    {
        const int HEADER = 8 + 20 * 2 + 2;
        if (body.Length < HEADER) { Debug.LogWarning("[Client] ChatMessage body too short."); return; }

        long   accountNum = BitConverter.ToInt64(body, 0);
        string nickname   = ReadFixedUtf16(body, 8, 20);
        ushort msgLen     = BitConverter.ToUInt16(body, 48);

        string message = "";
        int msgBytes = msgLen * 2;
        if (body.Length >= HEADER + msgBytes)
            message = Encoding.Unicode.GetString(body, HEADER, msgBytes);

        OnChatMessage?.Invoke(accountNum, nickname, message);
    }

    // ════════════════════════════════════════════════════════════════════
    // STEP 4 : 게임 시작 준비 (게임 씬)
    // ════════════════════════════════════════════════════════════════════

    /// <summary>
    /// 게임 씬 진입 시 1회 호출. 게임 서버에 REQ_GAME_READY를 보내고,
    /// 결과는 OnGameReadyResponse 이벤트(RES_GAME_READY 수신)로 통지된다.
    /// </summary>
    public void RequestGameReady()
    {
        if (State != NetState.GameReady || !IsConnected)
        {
            Debug.LogWarning("[Client] RequestGameReady called but not connected to game server.");
            return;
        }

        var body = new byte[8];
        Buffer.BlockCopy(BitConverter.GetBytes(_accountNum), 0, body, 0, 8);
        SendToServer((ushort)PacketID.TETRIS_REQ_GAME_READY, body);
        Debug.Log("[Client] REQ_GAME_READY sent.");
    }

    // S -> C 게임 시작 준비 완료 응답. body: Status(1)
    void HandleGameReadyRes(byte[] body)
    {
        bool ok = body.Length > 0 && body[0] == 1;
        Debug.Log($"[Client] RES_GAME_READY received. Status={(ok ? "OK" : "FAIL")}");
        OnGameReadyResponse?.Invoke(ok);
    }

    /// <summary>
    /// 게임 결과 화면에서 로비로 돌아갈 때 1회 호출. 게임 서버에 REQ_GAME_RETURNCHAT을 보내고,
    /// 결과는 OnReturnChatResponse 이벤트(RES_GAME_RETURNCHAT 수신)로 통지된다.
    /// </summary>
    public void SendReturnChatRequest()
    {
        if (State != NetState.GameReady || !IsConnected)
        {
            Debug.LogWarning("[Client] SendReturnChatRequest called but not connected to game server.");
            return;
        }

        SendToServer((ushort)PacketID.TETRIS_REQ_GAME_RETURNCHAT, System.Array.Empty<byte>());
        Debug.Log("[Client] REQ_GAME_RETURNCHAT sent.");
    }

    // S -> C 로비 복귀 요청 응답. body: Status(1)
    void HandleReturnChatRes(byte[] body)
    {
        bool ok = body.Length > 0 && body[0] == 1;
        Debug.Log($"[Client] RES_GAME_RETURNCHAT received. Status={(ok ? "OK" : "FAIL")}");
        OnReturnChatResponse?.Invoke(ok);
    }

    // S -> C 카운트다운 시작. body: WORD Count(2, 초 단위)
    void HandleCountdown(byte[] body)
    {
        if (body.Length < 2)
        {
            Debug.LogWarning("[Client] Countdown body too short.");
            return;
        }

        ushort count = BitConverter.ToUInt16(body, 0);
        Debug.Log($"[Client] ACK_COUNTDOWN received. Count={count}");
        OnCountdown?.Invoke(count);
    }

    /// <summary>
    /// 매칭 버튼. body: AccountNum(8) + Nickname[20](UTF-16). 서버가 세션ID로만 유저를 식별하고
    /// 이 body를 읽지 않는 것을 확인했지만(TetrisServer_Message.cpp::MessageProc_MatchingReq),
    /// 와이어 포맷은 스펙대로 채워 보낸다.
    /// </summary>
    public void SendMatchRequest()
    {
        if (State != NetState.GameReady || !IsConnected)
        {
            Debug.LogWarning("[Client] SendMatchRequest called but not connected to game server.");
            return;
        }

        var body = new byte[8 + NICK_FIELD_LEN * 2];
        Buffer.BlockCopy(BitConverter.GetBytes(_accountNum), 0, body, 0, 8);
        WriteFixedUtf16(body, 8, MyNickname, NICK_FIELD_LEN);
        SendToServer((ushort)PacketID.TETRIS_REQ_MATCHING, body);
    }

    // S -> C 매칭 요청 응답. body: Status(1) — 0:실패 1:성공(대기열 진입)
    void HandleMatchingRes(byte[] body)
    {
        bool ok = body.Length > 0 && body[0] == 1;
        Debug.Log($"[Client] RES_MATCHING received. Status={(ok ? "OK" : "FAIL")}");
        OnMatchResponse?.Invoke(ok);
    }

    /// <summary>
    /// 매칭 중일 때 매칭 버튼을 다시 누르면 호출. body: 본문 없음.
    /// 결과는 OnMatchCancelResponse 이벤트(RES_MATCHING_CANCEL 수신)로 통지된다.
    /// </summary>
    public void SendMatchCancelRequest()
    {
        if (State != NetState.GameReady || !IsConnected)
        {
            Debug.LogWarning("[Client] SendMatchCancelRequest called but not connected to game server.");
            return;
        }

        SendToServer((ushort)PacketID.TETRIS_REQ_MATCHING_CANCEL, System.Array.Empty<byte>());
        Debug.Log("[Client] REQ_MATCHING_CANCEL sent.");
    }

    // S -> C 매칭 취소 요청 응답. body: Status(1) — 0:실패 1:성공
    void HandleMatchCancelRes(byte[] body)
    {
        bool ok = body.Length > 0 && body[0] == 1;
        Debug.Log($"[Client] RES_MATCHING_CANCEL received. Status={(ok ? "OK" : "FAIL")}");
        OnMatchCancelResponse?.Invoke(ok);
    }

    // S -> C 매칭 성공(상대방 확정). body: AccountNum(8) + OpAccountNum(8) + OpNickname[20](UTF-16)
    void HandleMatchingSuccess(byte[] body)
    {
        const int LEN = 8 + 8 + 20 * 2;
        if (body.Length < LEN)
        {
            Debug.LogWarning("[Client] MatchingSuccess body too short.");
            return;
        }

        long   opAccountNum = BitConverter.ToInt64(body, 8);
        string opNickname   = ReadFixedUtf16(body, 16, 20);
        Debug.Log($"[Client] RES_MATCHING_SUCCESS received. Opponent={opNickname}({opAccountNum})");
        OnMatchSuccess?.Invoke(opAccountNum, opNickname);
    }

    /// <summary>
    /// 게임 중 키 입력을 서버에 전송한다. body: DWORD InputType (en_INPUT_TYPE) — AccountNum 없음(세션ID로 식별).
    /// </summary>
    public void SendGameInput(en_INPUT_TYPE inputType)
    {
        if (State != NetState.GameReady || !IsConnected)
        {
            Debug.LogWarning("[Client] SendGameInput called but not connected to game server.");
            return;
        }

        var body = BitConverter.GetBytes((uint)inputType);
        SendToServer((ushort)PacketID.TETRIS_ACK_GAME_USERINPUT, body);
    }

    // S -> C 보드 스냅샷. body: BYTE HoldingBlock(1) + WORD NextBlockBag[5](10) + BYTE MyBoard[200] + BYTE OpponentBoard[200]
    const int BOARD_CELL_COUNT = 20 * 10; // Board.Width * Board.Height
    const int NEXT_BAG_COUNT   = 5;

    void HandleBoardUpdate(byte[] body)
    {
        const int MIN_LEN = 1 + NEXT_BAG_COUNT * 2 + BOARD_CELL_COUNT * 2;
        if (body.Length < MIN_LEN)
        {
            Debug.LogWarning("[Client] BoardUpdate body too short.");
            return;
        }

        var holdingBlock = (TetBlockType)body[0];
        int offset = 1;

        var nextBag = new TetBlockType[NEXT_BAG_COUNT];
        for (int i = 0; i < NEXT_BAG_COUNT; i++)
        {
            nextBag[i] = (TetBlockType)BitConverter.ToUInt16(body, offset);
            offset += 2;
        }

        var myBoard = new byte[BOARD_CELL_COUNT];
        Buffer.BlockCopy(body, offset, myBoard, 0, BOARD_CELL_COUNT);
        offset += BOARD_CELL_COUNT;

        var opponentBoard = new byte[BOARD_CELL_COUNT];
        Buffer.BlockCopy(body, offset, opponentBoard, 0, BOARD_CELL_COUNT);

        OnBoardUpdate?.Invoke(holdingBlock, nextBag, myBoard, opponentBoard);
    }

    // S -> C 낙하 중인 블록 갱신. body: IsSelf(1, 서버 필드명은 IsOpponent지만 1=자기 자신 블록) + BlockType(1) + Rotate(1) + X(1, signed char) + Y(1, signed char)
    void HandleBlockUpdate(byte[] body)
    {
        if (body.Length < 5)
        {
            Debug.LogWarning("[Client] BlockUpdate body too short.");
            return;
        }

        bool isSelf   = body[0] != 0;
        var blockType = (TetBlockType)body[1];
        byte rotate   = body[2];
        sbyte x       = unchecked((sbyte)body[3]);
        sbyte y       = unchecked((sbyte)body[4]);

        OnBlockUpdate?.Invoke(isSelf, blockType, rotate, x, y);
    }

    // S -> C 대기 가비지(데미지) 갱신. body: DamageCount(1)
    void HandleDamageUpdate(byte[] body)
    {
        if (body.Length < 1)
        {
            Debug.LogWarning("[Client] DamageUpdate body too short.");
            return;
        }

        int count = body[0];
        OnDamageUpdate?.Invoke(count);
    }

    // S -> C 게임 결과 통지. body: GameResultFlag(1) — 1:승리 0:패배
    void HandleGameResult(byte[] body)
    {
        if (body.Length < 1)
        {
            Debug.LogWarning("[Client] GameResult body too short.");
            return;
        }

        bool isWin = body[0] == 1;
        OnGameResult?.Invoke(isWin);
    }

    // ════════════════════════════════════════════════════════════════════
    // 연결 종료
    // ════════════════════════════════════════════════════════════════════

    public void Disconnect()
    {
        // 진단: Disconnect 호출 시 호출 주체 출력 (마치면 제거)
        Debug.LogWarning("[Client] Disconnect() called.\n" + System.Environment.StackTrace);
        StopHeartbeat(); // 디스콜 시 하트비트 중지
        CloseSocket();
        State = NetState.Idle;
        _chatRoster.Clear(); // 재로그인 시 유령(stale) 유저가 남지 않도록 초기화
        Debug.Log("[Client] Disconnected.");
    }

    bool RequireLoginReady(Action<bool> onFail)
    {
        if (State == NetState.LoginReady && IsConnected) return true;
        Debug.LogWarning("[Client] Not connected to login server. Call ConnectToLoginServer() first.");
        onFail?.Invoke(false);
        return false;
    }

    // ════════════════════════════════════════════════════════════════════
    // ── ASYNC RECEIVE ─────────────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    void StartReceive()
    {
        if (_socket == null || !_socket.Connected) return;
        try
        {
            _socket.BeginReceive(_recvBuf, 0, _recvBuf.Length, SocketFlags.None, OnReceive, null);
        }
        catch (Exception e)
        {
            DispatchError("BeginReceive failed: " + e.Message);
        }
    }

    void OnReceive(IAsyncResult ar)
    {
        // 진입 시 _socket을 로친에 캐쳓한다.
        // ParsePackets 도중 BeginConnectToGame이 _socket을 게임 서버용으로 교체해도
        // 이 콜백의 남은 실행은 직접 로그인 서버 소켓(sock)을 사용한다.
        var sock = _socket;
        if (sock == null) return;
        try
        {
            int received = sock.EndReceive(ar);   // _socket이 아닌 콗 소켓으로 EndReceive
            if (received == 0)
            {
                // 게임 서버 전환 중 로그인 서버가 FIN을 보내는 것은 정상 동작
                // 소켓이 이미 교체된 경우 조용히 무시한다
                if (ReferenceEquals(sock, _socket))
                    DispatchError("Server closed the connection.");
                return;
            }
            if (_pendingSize + received > _pendingBuf.Length)
            {
                DispatchError("Receive buffer overflow.");
                return;
            }
            Buffer.BlockCopy(_recvBuf, 0, _pendingBuf, _pendingSize, received);
            _pendingSize += received;
            ParsePackets();
            // _socket이 교체된 경우(= 게임 서버 전환) StartReceive는 OnConnected에서 호용하므로 여기서는 제외
            if (ReferenceEquals(sock, _socket))
                StartReceive();
        }
        catch (ObjectDisposedException) { /* 소켓 닫힘, 무시 */ }
        catch (SocketException se) when (
            se.SocketErrorCode == SocketError.OperationAborted ||
            se.SocketErrorCode == SocketError.Interrupted      ||
            se.SocketErrorCode == SocketError.ConnectionReset)
        { /* 소켓 강제 닫힘으로 인한 정상적인 종료, 무시 */ }
        catch (Exception e)
        {
            if (ReferenceEquals(sock, _socket))
                DispatchError("OnReceive error: " + e.Message);
        }
    }

    
    // 헤더: [FixedKey(1)][shLen(2,LE)][RandKey(1)][CheckSum(1, 암호화됨)] + payload(shLen, 암호화됨)
    void ParsePackets()
    {
        int consumed = 0;
        while (consumed + HEADER_SIZE <= _pendingSize)
        {
            int shLen = _pendingBuf[consumed + 1] | (_pendingBuf[consumed + 2] << 8);
            if (shLen < 0 || shLen > PROTOCOL_MAX_SIZE)
            {
                DispatchError("비정상 패킷 크기: " + shLen);
                return;
            }

            int totalLen = HEADER_SIZE + shLen;
            if (consumed + totalLen > _pendingSize) break; // 더 받아야 함

            byte randKey = _pendingBuf[consumed + 3];

            // CheckSum(1) + payload(shLen) 구간을 복사해서 복호화 (원본 버퍼 훼손 방지)
            var region = new byte[1 + shLen];
            Buffer.BlockCopy(_pendingBuf, consumed + 4, region, 0, region.Length);
            DecodeInPlace(region, FIXED_KEY, randKey);

            byte decodedChecksum = region[0];
            byte expected = ComputeChecksum(region, 1, shLen);
            if (decodedChecksum != expected)
            {
                DispatchError("체크섬 불일치 (패킷 손상 또는 암호화 키 불일치)");
                return;
            }

            if (shLen < 2)
            {
                DispatchError("패킷이 PacketID를 담기에 너무 작습니다.");
                return;
            }

            ushort packetType = (ushort)(region[1] | (region[2] << 8));
            int bodyLen = shLen - 2;
            var body = new byte[bodyLen];
            if (bodyLen > 0) Buffer.BlockCopy(region, 3, body, 0, bodyLen);

            _mainQueue.Enqueue(() => Dispatch(packetType, body));

            consumed += totalLen;
        }

        if (consumed > 0 && consumed < _pendingSize)
            Buffer.BlockCopy(_pendingBuf, consumed, _pendingBuf, 0, _pendingSize - consumed);
        _pendingSize = Math.Max(0, _pendingSize - consumed);
    }

    void Dispatch(ushort packetID, byte[] body)
    {
        switch ((PacketID)packetID)
        {
            case PacketID.TETRISLOGIN_RES_DUPCHECK:
                HandleDupCheckRes(body);
                break;
            case PacketID.TETRISLOGIN_RES_REGISTER:
                HandleRegisterRes(body);
                break;
            case PacketID.TETRISLOGIN_RES_LOGIN:     // 로그인서버의 전체 응답 (서버 태그 스왑 수정 반영)
                HandleLoginServerFullRes(body);
                break;
            case PacketID.TETRIS_RES_LOGIN:          // 게임서버의 단순 응답 (서버 태그 스왑 수정 반영)
                HandleGameServerSimpleRes(body);
                break;
            case PacketID.TETRIS_ACK_CHAT_ENTER:
                HandleChatEnter(body);
                break;
            case PacketID.TETRIS_ACK_CHAT_EXIT:
                HandleChatExit(body);
                break;
            case PacketID.TETRIS_REQ_CHAT_MESSAGE:   // 서버가 같은 태그를 브로드캐스트에도 재사용함
                HandleChatMessage(body);
                break;
            case PacketID.TETRIS_RES_MATCHING:
                HandleMatchingRes(body);
                break;
            case PacketID.TETRIS_RES_MATCHING_CANCEL:
                HandleMatchCancelRes(body);
                break;
            case PacketID.TETRIS_RES_MATCHING_SUCCESS:
                HandleMatchingSuccess(body);
                break;
            case PacketID.TETRIS_RES_GAME_READY:
                HandleGameReadyRes(body);
                break;
            case PacketID.TETRIS_SC_ACK_COUNTDOWN:
                HandleCountdown(body);
                break;
            case PacketID.TETRIS_SC_ACK_GAME_BOARDUPDATE:
                HandleBoardUpdate(body);
                break;
            case PacketID.TETRIS_SC_ACK_GAME_BLOCKUPDATE:
                HandleBlockUpdate(body);
                break;
            case PacketID.TETRIS_ACK_GAME_DAMAGE:
                HandleDamageUpdate(body);
                break;
            case PacketID.TETRIS_ACK_GAME_RESULT:
                HandleGameResult(body);
                break;
            case PacketID.TETRIS_RES_GAME_RETURNCHAT:
                HandleReturnChatRes(body);
                break;
            default:
                Debug.LogWarning("[Client] Unhandled packetID: " + packetID);
                break;
        }
    }

    void HandleDupCheckRes(byte[] body)
    {
        bool available = body.Length > 0 && body[0] == 1;

        var cb = _pendingDupCheckCb;
        _pendingDupCheckCb   = null;
        _pendingDupCheckKind = DupCheckKind.None;
        cb?.Invoke(available);
    }

    void HandleRegisterRes(byte[] body)
    {
        bool ok = body.Length > 0 && body[0] == 1;

        var cb = _pendingRegisterCb;
        _pendingRegisterCb = null;
        cb?.Invoke(ok);
    }

    // ════════════════════════════════════════════════════════════════════
    // ── ASYNC SEND ────────────────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    void SendToServer(ushort packetType, byte[] body)
    {
        if (!IsConnected)
        {
            Debug.LogWarning("[Client] SendToServer called but not connected.");
            return;
        }

        var payload = new byte[2 + body.Length];
        payload[0] = (byte)(packetType & 0xFF);
        payload[1] = (byte)((packetType >> 8) & 0xFF);
        Buffer.BlockCopy(body, 0, payload, 2, body.Length);

        var packet = BuildEncodedPacket(payload);
        _sendQueue.Enqueue(packet);
        lock (_sendLock) { if (!_isSending) SendNext(); }
    }

    void SendNext()
    {
        if (!_sendQueue.TryDequeue(out byte[] data)) return;
        _isSending = true;
        try
        {
            // 진단: 송신 직전 소켓 상태 확인 (마치면 제거)
            Debug.Log($"[Client] BeginSend: socket.Connected={_socket?.Connected}, State={State}, bytes={data.Length}");
            _socket.BeginSend(data, 0, data.Length, SocketFlags.None, OnSent, null);
        }
        catch (Exception e)
        {
            _isSending = false;
            DispatchError("BeginSend failed: " + e.Message);
        }
    }

    void OnSent(IAsyncResult ar)
    {
        try { _socket.EndSend(ar); }
        catch (ObjectDisposedException)
        {
            // 소켓 닫힘 후 남은 콜백, 무시
            lock (_sendLock) { _isSending = false; }
            return;
        }
        catch (SocketException se) when (
            se.SocketErrorCode == SocketError.ConnectionAborted ||
            se.SocketErrorCode == SocketError.ConnectionReset   ||
            se.SocketErrorCode == SocketError.OperationAborted  ||
            se.SocketErrorCode == SocketError.Interrupted)
        {
            // 연결 종료로 인한 송신 실패 — 플래그만 정리하고 추가 시도 안 함
            lock (_sendLock) { _isSending = false; }
            return;
        }
        catch (Exception e)
        {
            // 예상치 못한 오류 — DispatchError로 연결 정리
            lock (_sendLock) { _isSending = false; }
            DispatchError("OnSent error: " + e.Message);
            return;
        }
        finally
        {
            lock (_sendLock)
            {
                if (_sendQueue.IsEmpty) _isSending = false;
                else                    SendNext();
            }
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // ── 암/복호화 (서버 CPacket::Encode/Decode 와 동일한 알고리즘) ───────
    // ════════════════════════════════════════════════════════════════════

    static byte[] BuildEncodedPacket(byte[] payload)
    {
        int shLen = payload.Length;
        var buf = new byte[HEADER_SIZE + shLen];

        buf[0] = PROGRAM_KEY;
        buf[1] = (byte)(shLen & 0xFF);
        buf[2] = (byte)((shLen >> 8) & 0xFF);
        byte randKey = (byte)UnityEngine.Random.Range(0, 256);
        buf[3] = randKey;

        Buffer.BlockCopy(payload, 0, buf, HEADER_SIZE, shLen);
        buf[4] = ComputeChecksum(buf, HEADER_SIZE, shLen); // CheckSum은 payload만 대상으로 계산

        // CheckSum 바이트부터 payload 끝까지(1+shLen 바이트)를 암호화
        EncodeInPlace(buf, 4, 1 + shLen, FIXED_KEY, randKey);

        return buf;
    }

    static byte ComputeChecksum(byte[] buf, int offset, int len)
    {
        int sum = 0;
        for (int i = 0; i < len; i++) sum += buf[offset + i];
        return (byte)(sum % 256);
    }

    // buf[start .. start+len) 구간을 서버 CPacket::Encode(K, RK)와 동일하게 암호화
    static void EncodeInPlace(byte[] buf, int start, int len, byte K, byte RK)
    {
        byte E = 0, P = 0;
        int cnt = 1;
        for (int i = 0; i < len; i++)
        {
            byte D = buf[start + i];
            int sumP = P + RK + cnt;
            P = (byte)(D ^ sumP);
            int sumE = E + K + cnt;
            E = (byte)(P ^ sumE);
            buf[start + i] = E;
            cnt++;
        }
    }

    // region[0..region.Length)을 서버 CPacket::Decode(K, RK)와 동일하게 복호화 (in-place)
    static void DecodeInPlace(byte[] region, byte K, byte RK)
    {
        byte prevE = 0, prevP = 0;
        int cnt = 1;
        for (int i = 0; i < region.Length; i++)
        {
            byte E = region[i];
            int sumP = prevE + K + cnt;
            byte P = (byte)(E ^ sumP);
            int sumD = prevP + RK + cnt;
            byte D = (byte)(P ^ sumD);
            prevP = P;
            prevE = E;
            region[i] = D;
            cnt++;
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // ── HELPERS ───────────────────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════

    // 서버 char[fieldLen] (+ strcpy_s / GetData) 필드용 — ASCII, 남는 자리는 0(널)으로 패딩.
    static void WriteFixedAscii(byte[] dest, int offset, string s, int fieldLen)
    {
        if (s == null) s = "";
        byte[] raw = Encoding.ASCII.GetBytes(s);
        int copyLen = Math.Min(raw.Length, fieldLen);
        Buffer.BlockCopy(raw, 0, dest, offset, copyLen);
        // dest는 항상 새로 할당된 byte[]를 넘기는 전제라 나머지는 이미 0으로 채워져 있음.
    }

    // 서버 WCHAR[charCount] (UTF-16LE) 필드용 — 남는 자리는 0(널)으로 패딩.
    static void WriteFixedUtf16(byte[] dest, int offset, string s, int charCount)
    {
        if (s == null) s = "";
        byte[] raw = Encoding.Unicode.GetBytes(s);
        int copyLen = Math.Min(raw.Length, charCount * 2);
        Buffer.BlockCopy(raw, 0, dest, offset, copyLen);
        // dest는 항상 새로 할당된 byte[]를 넘기는 전제라 나머지는 이미 0으로 채워져 있음.
    }

    // 서버 WCHAR[charCount] (UTF-16LE) 필드용 — null 종료 지점까지만 잘라서 반환.
    static string ReadFixedUtf16(byte[] src, int offset, int charCount)
    {
        string s = Encoding.Unicode.GetString(src, offset, charCount * 2);
        int nullIdx = s.IndexOf('\0');
        return nullIdx >= 0 ? s.Substring(0, nullIdx) : s;
    }

    static Socket NewSocket()
    {
        var s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        s.NoDelay = true;
        return s;
    }

    void CloseSocket()
    {
        if (_socket == null) return;
        try { _socket.Shutdown(SocketShutdown.Both); } catch { }
        try { _socket.Close(); }                      catch { }
        _socket    = null;
        _isSending = false;
    }

    void DispatchError(string msg)
    {
        // 이미 정리된 상태면 재진입 방지 (소켓 닫힘 직후 다른 콜백이 다시 화재되는 코너 케이스)
        if (State == NetState.Idle) return;
        Debug.LogError("[Client] " + msg);

        // 진행 중이던 요청이 있으면 매달린 채로 두지 말고 실패로 정리한다.
        var loginCb = _pendingLoginCb;
        _pendingLoginCb = null; 
        var dupCb = _pendingDupCheckCb;
        _pendingDupCheckCb = null;
        _pendingDupCheckKind = DupCheckKind.None;
        var regCb = _pendingRegisterCb;
        _pendingRegisterCb = null;

        CloseSocket();
        State = NetState.Idle;

        _mainQueue.Enqueue(() =>
        {
            loginCb?.Invoke(false, msg);
            dupCb?.Invoke(false);
            regCb?.Invoke(false);
            OnDisconnected?.Invoke(msg);
        });
    }
}
