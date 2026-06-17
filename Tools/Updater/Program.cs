using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Threading.Tasks;

namespace Updater
{
    class Program
    {
        static async Task Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.WriteLine("エラー: ダウンロードURLが指定されていません。");
                Console.WriteLine("このプログラムはSae本体から自動的に呼び出されます。");
                Console.ReadLine();
                return;
            }

            string downloadUrl = args[0];
            string zipPath = "update.zip";

            Console.WriteLine("========================================");
            Console.WriteLine(" Sae Updater");
            Console.WriteLine("========================================");
            Console.WriteLine("アップデートをダウンロードしています...");

            // 本体 (Sae.exe) が完全に終了するのを待つ (少し長めに3秒待機)
            System.Threading.Thread.Sleep(3000);

            try
            {
                // 1. ダウンロード
                using (var client = new HttpClient())
                {
                    client.DefaultRequestHeaders.Add("User-Agent", "Sae Updater/1.0");
                    var response = await client.GetAsync(downloadUrl);
                    response.EnsureSuccessStatusCode();
                    using (var fs = new FileStream(zipPath, FileMode.Create))
                    {
                        await response.Content.CopyToAsync(fs);
                    }
                }

                // 2. 解凍・上書き
                Console.WriteLine("ファイルを展開し、上書きしています...");
                using (var archive = ZipFile.OpenRead(zipPath))
                {
                    foreach (var entry in archive.Entries)
                    {
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

                        Directory.CreateDirectory(Path.GetDirectoryName(destPath));
                        
                        // 実行中のUpdater.exe自身は上書きしない
                        if (entry.Name.Equals("Updater.exe", StringComparison.OrdinalIgnoreCase))
                            continue;
                        
                        // 上書き解凍
                        entry.ExtractToFile(destPath, true);
                    }
                }

                // 3. ゴミ掃除
                File.Delete(zipPath);
                Console.WriteLine("アップデートが完了しました！");
                Console.WriteLine("アプリを再起動します...");
                System.Threading.Thread.Sleep(1000);

                // 4. 本体再起動
                var psi = new ProcessStartInfo
                {
                    FileName = "Sae.exe",
                    UseShellExecute = true
                };
                Process.Start(psi);
            }
            catch (Exception ex)
            {
                Console.WriteLine("アップデート中にエラーが発生しました: " + ex.Message);
                Console.WriteLine("Enterキーを押して終了してください。");
                Console.ReadLine();
            }
        }
    }
}
