using System;
using System.Collections.Generic;
using System.IO.MemoryMappedFiles;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Drawing;
using System.Runtime.InteropServices;

namespace EngineProfiler
{
    [StructLayout(LayoutKind.Sequential, Pack = 4, CharSet = CharSet.Ansi)]
    public unsafe struct SharedEngineData
    {
        public uint frameNumber;
        public float fps;
        public float deltaTime;
        public uint drawCalls;
        public uint particleCount;
        public uint lightCount;
        public float playerX;
        public float playerY;
        public float playerZ;
        
        // プロ向け追加データ
        public float cpuLogicTimeMs;
        public float gpuRenderTimeMs;
        public float systemRamUsageMB;
        public float videoRamUsageMB;
        public fixed byte eventMarker[64];
        
        public string GetEventMarker()
        {
            fixed (byte* p = eventMarker)
            {
                return new string((sbyte*)p).Trim('\0');
            }
        }
    }

    public class ProfilerForm : Form
    {
        private Label lblStatus;
        private Label lblRealtimeData;
        private TextBox txtLog;
        private ScottPlot.WinForms.FormsPlot formsPlot; // ★追加: グラフ描画用コントロール
        private ScottPlot.Plottables.DataStreamer streamerFps;
        private ScottPlot.Plottables.DataStreamer streamerDrawCalls;
        
        private MemoryMappedFile? mmf = null;
        private MemoryMappedViewAccessor? accessor = null;
        private uint lastProcessedFrame = 0;
        
        private List<SharedEngineData> sampledData = new List<SharedEngineData>();
        private DateTime lastAnalysisTime = DateTime.Now;
        private static readonly HttpClient httpClient = new HttpClient();
        private bool isAnalyzing = false;

        public ProfilerForm()
        {
            // ウィンドウの初期設定
            this.Text = "NeoEngine AI Profiler - Professional Edition";
            this.Size = new Size(1200, 800); // グラフを置くため少し大きく
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(30, 30, 30);
            this.ForeColor = Color.White;

            // ステータスラベル
            lblStatus = new Label() {
                Text = "エンジン待機中...",
                Location = new Point(20, 20),
                AutoSize = true,
                Font = new Font("Segoe UI", 12, FontStyle.Bold),
                ForeColor = Color.Yellow
            };
            this.Controls.Add(lblStatus);

            // リアルタイムデータラベル
            lblRealtimeData = new Label() {
                Text = "FPS: 0\nDrawCalls: 0\n...",
                Location = new Point(20, 60),
                AutoSize = true, 
                Font = new Font("Consolas", 12),
                ForeColor = Color.LightGreen
            };
            this.Controls.Add(lblRealtimeData);

            // グラフコントロール
            formsPlot = new ScottPlot.WinForms.FormsPlot() {
                Location = new Point(450, 20),
                Size = new Size(700, 300),
                Anchor = AnchorStyles.Top | AnchorStyles.Right | AnchorStyles.Left
            };
            this.Controls.Add(formsPlot);
            
            // グラフをダークテーマで見やすく調整
            formsPlot.Plot.FigureBackground.Color = ScottPlot.Color.FromHex("#252526");
            formsPlot.Plot.DataBackground.Color = ScottPlot.Color.FromHex("#1e1e1e");
            formsPlot.Plot.Axes.Color(ScottPlot.Colors.LightGray); // 軸の文字や線を明るいグレーに
            formsPlot.Plot.Grid.MajorLineColor = ScottPlot.Color.FromHex("#404040"); // グリッド線を少し明るく
            
            streamerFps = formsPlot.Plot.Add.DataStreamer(600);
            streamerFps.Color = ScottPlot.Colors.LimeGreen;
            streamerDrawCalls = formsPlot.Plot.Add.DataStreamer(600);
            streamerDrawCalls.Color = ScottPlot.Colors.Cyan;

            // ログ出力用テキストボックス
            txtLog = new TextBox() {
                Multiline = true,
                ScrollBars = ScrollBars.Vertical,
                Location = new Point(20, 340), // グラフの下に配置
                Size = new Size(1130, 400),
                Font = new Font("Consolas", 11), 
                BackColor = Color.FromArgb(20, 20, 20),
                ForeColor = Color.LightGray,
                ReadOnly = true,
                Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right
            };
            this.Controls.Add(txtLog);

            // OnLoadで非同期ループを開始します
            Log("プロファイラを起動しました。C++エンジンの起動を待っています...");
            Log("※現在は自動分析モード（15秒間隔）で動作しています。");
        }

        protected override async void OnLoad(EventArgs e)
        {
            base.OnLoad(e);
            await PollingLoopAsync();
        }

        private async Task PollingLoopAsync()
        {
            while (!this.IsDisposed)
            {
                await Task.Delay(16); // 60fps間隔で監視

                // 共有メモリに接続
                if (mmf == null)
                {
                    try
                    {
                        mmf = MemoryMappedFile.OpenExisting("NeoEngineSharedMemory");
                        accessor = mmf.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
                        lblStatus.Text = "接続済み：リアルタイム監視中";
                        lblStatus.ForeColor = Color.Lime;
                        Log("エンジンとの共有メモリ接続に成功しました！");
                    }
                    catch (System.IO.FileNotFoundException)
                    {
                        // まだエンジンが立ち上がっていない（正常な待機状態）
                        continue;
                    }
                    catch (Exception ex)
                    {
                        Log($"[接続エラー] {ex.GetType().Name}: {ex.Message}");
                        await Task.Delay(1000); // エラー時は少し長く待つ
                        continue;
                    }
                }

                if (accessor == null) continue;

                try
                {
                    // データの読み取り
                    SharedEngineData data;
                    accessor.Read(0, out data);

                    // フレームが進んでいたら記録
                    if (data.frameNumber > lastProcessedFrame)
                    {
                        lastProcessedFrame = data.frameNumber;
                        sampledData.Add(data);

                        // UIにリアルタイム反映
                        string marker = data.GetEventMarker();
                        lblRealtimeData.Text = 
                            $"FPS        : {data.fps:F1} (Delta: {data.deltaTime:F4})\n" +
                            $"CPU Time   : {data.cpuLogicTimeMs:F2} ms\n" +
                            $"GPU Time   : {data.gpuRenderTimeMs:F2} ms\n" +
                            $"SYS RAM    : {data.systemRamUsageMB:F1} MB\n" +
                            $"VRAM       : {data.videoRamUsageMB:F1} MB\n" +
                            $"DrawCalls  : {data.drawCalls}\n" +
                            $"Particles  : {data.particleCount}\n" +
                            $"Marker     : {(string.IsNullOrEmpty(marker) ? "None" : marker)}";

                        // グラフの更新 (FPSとDrawCall)
                        streamerFps.Add(data.fps);
                        streamerDrawCalls.Add(data.drawCalls);
                        formsPlot.Refresh();

                        // 15秒ごとに自動送信
                        if (!isAnalyzing && (DateTime.Now - lastAnalysisTime).TotalSeconds >= 15.0)
                        {
                            lastAnalysisTime = DateTime.Now;
                            var dataToSend = new List<SharedEngineData>(sampledData);
                            sampledData.Clear();
                            
                            isAnalyzing = true;
                            _ = AnalyzeAndExportAsync(dataToSend);
                        }
                    }
                }
                catch (Exception ex)
                {
                    Log($"[読み取りエラー] {ex.Message}");
                    mmf?.Dispose();
                    mmf = null;
                    accessor?.Dispose();
                    accessor = null;
                }
            }
        }

        private async Task AnalyzeAndExportAsync(List<SharedEngineData> logs)
        {
            if (logs.Count == 0) return;

            float minFps = 9999, maxFps = 0, sumFps = 0;
            foreach (var log in logs)
            {
                if (log.fps < minFps) minFps = log.fps;
                if (log.fps > maxFps) maxFps = log.fps;
                sumFps += log.fps;
            }
            float avgFps = sumFps / logs.Count;

            Log($"過去10秒間のデータを集計 (サンプル数: {logs.Count}) => FPS: Min {minFps:F1}, Max {maxFps:F1}, Avg {avgFps:F1}");
            
            var lastLog = logs[logs.Count - 1];
            Log($" --- 送信直前の最新データ ---");
            Log($"  DrawCalls: {lastLog.drawCalls}, Particles: {lastLog.particleCount}, Lights: {lastLog.lightCount}");
            Log($"  PlayerPos: ({lastLog.playerX:F2}, {lastLog.playerY:F2}, {lastLog.playerZ:F2})");

            // APIキーを取得 (api_key.txt を最優先で読み込む)
            string apiKey = "";
            if (System.IO.File.Exists("api_key.txt"))
            {
                apiKey = System.IO.File.ReadAllText("api_key.txt").Trim();
            }
            if (string.IsNullOrEmpty(apiKey))
            {
                apiKey = Environment.GetEnvironmentVariable("GROQ_API_KEY") ?? "";
            }

            if (string.IsNullOrEmpty(apiKey))
            {
                Log("[警告] APIキーが設定されていません。'api_key.txt' にキーを保存してください。");
                isAnalyzing = false;
                return;
            }

            // ★追加: 毎フレームの全データ（約600件）をそのまま送るとトークン数が30000を超えて
            // 無料枠の制限（6000 TPM）に引っかかるため、約15件に間引く（ダウンサンプリング）
            var downsampledLogs = new List<SharedEngineData>();
            int step = Math.Max(1, logs.Count / 15);
            for (int i = 0; i < logs.Count; i += step)
            {
                downsampledLogs.Add(logs[i]);
            }

            // ダウンサンプリングされたデータの中に、マーカー文字を変換しておく
            var formattedLogs = new List<object>();
            foreach (var log in downsampledLogs)
            {
                formattedLogs.Add(new {
                    log.frameNumber, log.fps, log.deltaTime, log.drawCalls, log.particleCount,
                    log.cpuLogicTimeMs, log.gpuRenderTimeMs, log.systemRamUsageMB, log.videoRamUsageMB,
                    EventMarker = log.GetEventMarker()
                });
            }

            var options = new JsonSerializerOptions { IncludeFields = true, WriteIndented = true };
            string jsonLog = JsonSerializer.Serialize(formattedLogs, options);

            // ★追加: ログをファイルに保存する
            string logDir = "Logs";
            if (!System.IO.Directory.Exists(logDir)) System.IO.Directory.CreateDirectory(logDir);
            string timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            string jsonFilePath = System.IO.Path.Combine(logDir, $"profile_data_{timestamp}.json");
            System.IO.File.WriteAllText(jsonFilePath, jsonLog);

            string prompt = $@"
あなたはプロのAAAゲームエンジンのパフォーマンスエンジニアです。
以下の15秒間のテレメトリデータ（JSON）を分析し、プロ向けの高度なレポートを日本語で作成してください。

【プロの分析必須項目】
1. ボトルネックの特定：FPSが落ちたフレームで、CPU（cpuLogicTimeMs）とGPU（gpuRenderTimeMs）のどちらが限界を迎えていたか？
2. メモリの健康状態：systemRamUsageMB または videoRamUsageMB が時間経過で徐々に増加（メモリリーク）していないか？
3. イベントとの相関：EventMarker（イベントマーカー）が発行されたフレームやその直後に、スパイク（負荷急増）が発生していないか？

データ全体を通して異常やボトルネックがなければ、「プロの目から見てもパフォーマンスとメモリは完全に安定しています」と報告してください。

【ログデータ】
{jsonLog}
";

            // OpenAI互換（Groq API等）のペイロード構築
            var payload = new
            {
                model = "llama-3.1-8b-instant", // Groqの最新の超高速・無料モデル
                messages = new[]
                {
                    new { role = "user", content = prompt }
                }
            };

            string jsonPayload = JsonSerializer.Serialize(payload);
            var request = new HttpRequestMessage(HttpMethod.Post, "https://api.groq.com/openai/v1/chat/completions");
            request.Headers.Add("Authorization", $"Bearer {apiKey.Trim()}");
            request.Content = new StringContent(jsonPayload, Encoding.UTF8, "application/json");

            try
            {
                var response = await httpClient.SendAsync(request);
                string responseStr = await response.Content.ReadAsStringAsync();

                if (response.IsSuccessStatusCode)
                {
                    using var doc = JsonDocument.Parse(responseStr);
                    var text = doc.RootElement.GetProperty("choices")[0].GetProperty("message").GetProperty("content").GetString();
                    
                    Log("========== AI 分析レポート ==========");
                    Log(text ?? "");
                    Log("=====================================\n");

                    // ★追加: AIの回答もテキストファイルとして保存する
                    string reportFilePath = System.IO.Path.Combine(logDir, $"ai_report_{timestamp}.txt");
                    System.IO.File.WriteAllText(reportFilePath, text ?? "");
                    Log($"※ログと分析結果を '{logDir}' フォルダに保存しました。");
                }
                else
                {
                    Log($"[エラー] API通信エラー: {response.StatusCode}\n{responseStr}");
                }
            }
            catch (Exception ex)
            {
                Log($"[例外] API通信エラー: {ex.Message}");
            }
            isAnalyzing = false;
        }

        private void Log(string message)
        {
            if (txtLog.InvokeRequired)
            {
                txtLog.Invoke(new Action(() => Log(message)));
                return;
            }
            txtLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {message}\r\n");
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            accessor?.Dispose();
            mmf?.Dispose();
            base.OnFormClosed(e);
        }
    }

    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2); // ★追加: 高DPI（4K等の高解像度）環境での文字のぼやけを解消
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new ProfilerForm());
        }
    }
}
