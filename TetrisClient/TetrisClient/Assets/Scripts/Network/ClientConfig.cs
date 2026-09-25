using System.Collections.Generic;
using System.IO;
using System.Net;
using UnityEngine;

/// <summary>
/// 실행 파일 옆 config.txt에서 로그인 서버 주소를 읽는다. 포맷은 서버 CConfigReader와 동일
/// ("Key = Value" 또는 "Key : Value", #/; 주석, 값 양옆 큰따옴표 제거).
/// 빌드: TetrisClient.exe와 같은 폴더 / 에디터: Unity 프로젝트 루트(Assets 옆).
/// </summary>
public static class ClientConfig
{
    const string FILE_NAME = "config.txt";

    public static string LoginServerIP   { get; private set; }
    public static int    LoginServerPort { get; private set; }

    public static string FilePath => Path.Combine(Path.GetDirectoryName(Application.dataPath), FILE_NAME);

    public static bool Load(out string error)
    {
        string path = FilePath;
        if (!File.Exists(path))
        {
            error = $"config.txt를 찾을 수 없습니다. ({path})";
            return false;
        }

        var values = new Dictionary<string, string>();
        try
        {
            foreach (string line in File.ReadAllLines(path))
            {
                string trimmed = line.Trim();
                if (trimmed.Length == 0 || trimmed[0] == '#' || trimmed[0] == ';')
                    continue;

                int sep = trimmed.IndexOfAny(new[] { '=', ':' });
                if (sep < 0)
                    continue;

                string key   = trimmed.Substring(0, sep).Trim();
                string value = StripQuotes(trimmed.Substring(sep + 1).Trim());
                if (key.Length > 0)
                    values[key] = value;
            }
        }
        catch (IOException e)
        {
            error = $"config.txt를 읽지 못했습니다. ({e.Message})";
            return false;
        }

        if (!values.TryGetValue("LoginServerIP", out string ip) || !IPAddress.TryParse(ip, out _))
        {
            error = "config.txt의 LoginServerIP가 없거나 올바른 IP가 아닙니다.";
            return false;
        }

        if (!values.TryGetValue("LoginServerPort", out string portStr)
            || !int.TryParse(portStr, out int port) || port < 1 || port > 65535)
        {
            error = "config.txt의 LoginServerPort가 없거나 올바른 포트(1~65535)가 아닙니다.";
            return false;
        }

        LoginServerIP   = ip;
        LoginServerPort = port;
        error = null;
        return true;
    }

    static string StripQuotes(string s)
        => s.Length >= 2 && s[0] == '"' && s[s.Length - 1] == '"' ? s.Substring(1, s.Length - 2) : s;
}
