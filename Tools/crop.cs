using System;
using System.Drawing;
using System.Drawing.Imaging;

public class Program {
    public static void Main(string[] args) {
        string inPath = @"C:\Users\k024g\.gemini\antigravity\brain\abd8412f-e89f-49e8-afa0-95944fb37567\shimaenaga.png";
        string outPath = @"C:\Users\k024g\source\repos\Sae\Resources\Textures\shimaenaga.png";
        
        Bitmap bmp = new Bitmap(inPath);
        
        // We need one clean sprite. Let's look for IDLE 1.
        // The image is 1024x1024. 
        // IDLE 1 is around x=50, y=100.
        // Let's crop a 128x128 square around x=40, y=90.
        int x = 40;
        int y = 90;
        int w = 120;
        int h = 120;
        
        Bitmap crop = bmp.Clone(new Rectangle(x, y, w, h), bmp.PixelFormat);
        
        // Make the background transparent. The background is a checkerboard of white and light gray.
        // The bird is white but has a dark outline.
        // Simple flood fill from edges
        bool[,] visited = new bool[w, h];
        var q = new System.Collections.Generic.Queue<Point>();
        
        // Enqueue all edge points
        for(int i = 0; i < w; i++) {
            q.Enqueue(new Point(i, 0));
            q.Enqueue(new Point(i, h - 1));
        }
        for(int j = 0; j < h; j++) {
            q.Enqueue(new Point(0, j));
            q.Enqueue(new Point(w - 1, j));
        }
        
        while(q.Count > 0) {
            Point p = q.Dequeue();
            if (p.X < 0 || p.X >= w || p.Y < 0 || p.Y >= h) continue;
            if (visited[p.X, p.Y]) continue;
            
            Color c = crop.GetPixel(p.X, p.Y);
            // If it's bright (checkerboard is usually > 200)
            if (c.R > 230 && c.G > 230 && c.B > 230) {
                crop.SetPixel(p.X, p.Y, Color.Transparent);
                visited[p.X, p.Y] = true;
                q.Enqueue(new Point(p.X + 1, p.Y));
                q.Enqueue(new Point(p.X - 1, p.Y));
                q.Enqueue(new Point(p.X, p.Y + 1));
                q.Enqueue(new Point(p.X, p.Y - 1));
            } else {
                visited[p.X, p.Y] = true;
            }
        }
        
        crop.Save(outPath, ImageFormat.Png);
    }
}
