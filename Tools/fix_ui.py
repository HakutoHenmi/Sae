import re

def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    out_lines = []
    for line in lines:
        if 'contentH = 1650.0f;' in line:
            line = line.replace('1650.0f', '1900.0f')
        elif 'panelY +' in line:
            def replacer(match):
                y_val = float(match.group(1))
                if 680 <= y_val < 800:
                    y_val += 20
                elif 800 <= y_val < 830:
                    y_val += 40
                elif 830 <= y_val < 1020:
                    y_val += 70
                elif 1020 <= y_val < 1050:
                    y_val += 90
                elif 1050 <= y_val < 1220:
                    y_val += 120
                elif 1220 <= y_val < 1250:
                    y_val += 140
                elif 1250 <= y_val < 1470:
                    y_val += 170
                elif 1470 <= y_val < 1500:
                    y_val += 190
                elif y_val >= 1500:
                    y_val += 220
                return f"panelY + {y_val:.1f}f"
            
            line = re.sub(r'panelY \+ ([0-9]+(?:\.[0-9]+)?)f', replacer, line)
        
        # 音量がゼロに近い場合はゼロにする処理を追加する
        if 'if (handleSlider(9, panelX + 130.0f, panelY + 1365.0f' in line or 'if (handleSlider(9, panelX + 130.0f, panelY + 1535.0f' in line:
            pass # 後で手動または別途置換する方が安全
        
        out_lines.append(line)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.writelines(out_lines)

process_file('Game/Scenes/MainScene.cpp')
