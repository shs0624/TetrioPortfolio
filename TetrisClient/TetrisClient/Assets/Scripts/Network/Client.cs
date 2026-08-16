using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Collections.Concurrent;
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

    // ── Wire format constants (서버 Protocol.h / NetServer.h 와 반드시 일치해야 함) ──
    const byte FIXED_KEY   = 0x32;  // 서버 _FixedKey  (암/복호화 키)
    const byte PROGRAM_KEY = 0x77;  // 서버 _ProgramKey (헤더 FixedKey 필드에 실어 보내는 값)
    const int  HEADER_SIZE = 5;     // FixedKey(1) + shLen(2) + RandKey(1) + CheckSum(1)
    const int  PROTOCOL_MAX_SIZE = 500;

    const int ID_FIELD_LEN            = 20;  // char ID[20]
    const int PW_FIELD_LEN            = 20;  // char Passwd[20]
    const int NICK_FIELD_LEN          = 20;  // char Nickname[20]
    const int GAME_SESSIONKEY_LEN     = 64;  // 게임서버가 읽는 CHAR SessionKey[64]
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

    // ── Unity lifecycle ───────────────────────────────────────────────
    void Awake()
    {
        if (Instance != null && Instance != this) { Destroy(gameObject); return; }
        Instance = this;
        DontDestroyOnLoad(gameObject);
    }

    void Update()
    {
        while (_mainQueue.TryDequeue(out Action a)) a?.Invoke();
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
        CloseSocket();
        _pendingSize = 0;
        BeginConnectToGame(gameIP, gamePort);
    }

    void BeginConnectToGame(string ip, int port)
    {
        State = NetState.GameConnecting;
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
        var body = new byte[8 + GAME_SESSIONKEY_LEN];
        Buffer.BlockCopy(BitConverter.GetBytes(_accountNum), 0, body, 0, 8);
        WriteFixedAscii(body, 8, _sessionKey, GAME_SESSIONKEY_LEN);
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
    // 연결 종료
    // ════════════════════════════════════════════════════════════════════

    public void Disconnect()
    {
        CloseSocket();
        State = NetState.Idle;
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
        try
        {
            int received = _socket.EndReceive(ar);
            if (received <= 0)
            {
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
            StartReceive();
        }
        catch (ObjectDisposedException) { /* 소켓 닫힘, 무시 */ }
        catch (Exception e)
        {
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
        catch (Exception e) { Debug.LogError("[Client] OnSent error: " + e.Message); }
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
