using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Updater
{
    class Program
    {
        static Form? mainForm;
        static Label? statusLabel;
        static ProgressBar? progressBar;

        [STAThread]
        static void Main(string[] args)
        {
            ApplicationConfiguration.Initialize();

            if (args.Length == 0)
            {
                MessageBox.Show("エラー: ダウンロードURLが指定されていません。\nこのプログラムはSae本体から自動的に呼び出されます。", "Sae Updater", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            string downloadUrl = args[0];

            mainForm = new Form
            {
                Text = "Sae Updater",
                Size = new Size(420, 180),
                StartPosition = FormStartPosition.CenterScreen,
                FormBorderStyle = FormBorderStyle.FixedDialog,
                MaximizeBox = false,
                MinimizeBox = false,
                BackColor = Color.FromArgb(24, 27, 34), // ダークグレー
                ForeColor = Color.White
            };

            statusLabel = new Label
            {
                Text = "Sae の終了を待機しています...",
                Location = new Point(25, 25),
                Size = new Size(350, 30),
                Font = new Font("MS Gothic", 10, FontStyle.Regular),
                TextAlign = ContentAlignment.MiddleLeft
            };

            progressBar = new ProgressBar
            {
                Location = new Point(25, 70),
                Size = new Size(350, 25),
                Style = ProgressBarStyle.Continuous,
                Minimum = 0,
                Maximum = 100,
                Value = 0
            };

            mainForm.Controls.Add(statusLabel);
            mainForm.Controls.Add(progressBar);

            mainForm.Load += async (s, e) =>
            {
                try
                {
                    await RunUpdateAsync(downloadUrl);
                    mainForm.Close();
                }
                catch (Exception ex)
                {
                    MessageBox.Show("アップデート中にエラーが発生しました:\n" + ex.Message, "Sae Updater", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    mainForm.Close();
                }
            };

            Application.Run(mainForm);
        }

        static async Task RunUpdateAsync(string downloadUrl)
        {
            string zipPath = "update.zip";
            const string targetProcessName = "Sae";

            // 1. プロセス終了待機
            UpdateStatus("Sae の終了を待機しています...", 10);
            int retries = 30;
            while (retries > 0)
            {
                var processes = Process.GetProcessesByName(targetProcessName);
                if (processes.Length == 0)
                {
                    break;
                }
                await Task.Delay(200);
                retries--;
            }

            // それでも残っている場合は強制終了を試みる
            try
            {
                var remaining = Process.GetProcessesByName(targetProcessName);
                foreach (var p in remaining)
                {
                    p.Kill();
                    p.WaitForExit(1000);
                }
            }
            catch
            {
                // 無視
            }

            // 2. ダウンロード
            UpdateStatus("アップデートをダウンロードしています...", 20);
            using (var client = new HttpClient())
            {
                client.DefaultRequestHeaders.Add("User-Agent", "Sae Updater/1.0");
                using (var response = await client.GetAsync(downloadUrl, HttpCompletionOption.ResponseHeadersRead))
                {
                    response.EnsureSuccessStatusCode();
                    var contentLength = response.Content.Headers.ContentLength;

                    using (var downloadStream = await response.Content.ReadAsStreamAsync())
                    using (var fileStream = new FileStream(zipPath, FileMode.Create, FileAccess.Write, FileShare.None))
                    {
                        var buffer = new byte[8192];
                        long totalRead = 0;
                        int read;
                        while ((read = await downloadStream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                        {
                            await fileStream.WriteAsync(buffer, 0, read);
                            totalRead += read;
                            if (contentLength.HasValue)
                            {
                                int percentage = 20 + (int)((double)totalRead / contentLength.Value * 50); // ダウンロードは 20%〜70%
                                UpdateStatus($"アップデートをダウンロードしています... ({totalRead / 1024 / 1024}MB / {contentLength.Value / 1024 / 1024}MB)", percentage);
                            }
                        }
                    }
                }
            }

            // 3. 解凍・上書き
            UpdateStatus("ファイルを展開し、上書きしています...", 75);
            using (var archive = ZipFile.OpenRead(zipPath))
            {
                int totalEntries = archive.Entries.Count;
                int currentEntry = 0;
                foreach (var entry in archive.Entries)
                {
                    currentEntry++;
                    string destPath = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, entry.FullName));
                    
                    // ZipSlip 脆弱性対策
                    if (!destPath.StartsWith(AppDomain.CurrentDomain.BaseDirectory, StringComparison.Ordinal))
                        continue;

                    // ディレクトリの作成
                    if (entry.FullName.EndsWith("/") || entry.FullName.EndsWith("\\"))
                    {
                        Directory.CreateDirectory(destPath);
                        continue;
                    }

                    var dirName = Path.GetDirectoryName(destPath);
                    if (dirName != null)
                    {
                        Directory.CreateDirectory(dirName);
                    }
                    
                    // 実行中のUpdater.exe自身は上書きしない
                    if (entry.Name.Equals("Updater.exe", StringComparison.OrdinalIgnoreCase))
                        continue;
                    
                    // 上書き解凍
                    entry.ExtractToFile(destPath, true);

                    int percentage = 75 + (int)((double)currentEntry / totalEntries * 20); // 展開は 75%〜95%
                    UpdateStatus($"ファイルを展開しています... ({currentEntry}/{totalEntries})", percentage);
                }
            }

            // 4. ゴミ掃除
            UpdateStatus("後片付けを行っています...", 95);
            if (File.Exists(zipPath))
            {
                File.Delete(zipPath);
            }

            // 5. 本体再起動の合図
            UpdateStatus("アップデート完了！アプリを再起動します...", 100);
            await Task.Delay(1200);

            // 6. 本体再起動
            var psi = new ProcessStartInfo
            {
                FileName = "Sae.exe",
                UseShellExecute = true
            };
            Process.Start(psi);
        }

        static void UpdateStatus(string message, int progress)
        {
            if (mainForm != null && mainForm.InvokeRequired)
            {
                mainForm.Invoke(new Action(() => UpdateStatus(message, progress)));
                return;
            }
            if (statusLabel != null)
            {
                statusLabel.Text = message;
            }
            if (progressBar != null)
            {
                progressBar.Value = progress;
            }
        }
    }
}
