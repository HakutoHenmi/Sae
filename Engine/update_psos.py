import os

path = r'c:\Users\k024g\source\repos\neo_Engine\Engine\Renderer.cpp'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Line indices (0-indexed) based on previous grep (grep showed 1-indexed)
# 1133 -> 1132, etc.
targets = [1132, 1184, 1888, 1946, 1999, 2028, 2072, 2114, 2185, 2223]

for t in targets:
    if t < len(lines) and 'NumRenderTargets = 1;' in lines[t]:
        indent = lines[t][:lines[t].find('p')]
        var = 'pso' if 'pso.' in lines[t] else 'psoDesc'
        lines[t] = f"{indent}{var}.NumRenderTargets = 2;\n"
        lines[t] += f"{indent}{var}.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;\n"
    else:
        print(f"Skipping line {t+1}: {lines[t].strip() if t < len(lines) else 'OUT OF RANGE'}")

with open(path, 'w', encoding='utf-8') as f:
    f.writelines(lines)
print("Updated PSOs successfully.")
