import os

def normalize_path(p):
    return p.replace('/', os.sep).replace('\\', os.sep)

files = [
    'Game/Scripts/KamikazeEnemyScript.cpp',
    'Game/Scripts/PoisonTrap.cpp',
    'Game/Scripts/PlayerScript.cpp',
    'Game/Scripts/ExperienceHopper.cpp',
    'Game/Scripts/BaseScript.cpp',
    'Game/Scripts/EnemySpawnerScript.cpp',
    'Game/Scripts/Canon.cpp',
    'Game/Scripts/EnemyBehavior.cpp',
    'Game/Scripts/PipeScript.cpp',
    'Game/Systems/CleanupSystem.h',
    'Game/EnemySystem/NavigationManager.cpp',
    'Game/Scenes/GameScene.cpp'
]

tags = {
    'Untagged': 'Untagged',
    'Player': 'Player',
    'Enemy': 'Enemy',
    'Bullet': 'Bullet',
    'Sword': 'Sword',
    'PlayerSword': 'PlayerSword',
    'Projectile': 'Projectile',
    'Pipe': 'Pipe',
    'pipe': 'Pipe',
    'Canon': 'Canon',
    'Cannon': 'Cannon',
    'Wall': 'Wall',
    'BulletTank': 'BulletTank',
    'PipeCannon': 'PipeCannon',
    'pipe_cannon': 'PipeCannon',
    'Body': 'Body',
    'Head': 'Head',
    'Explosion': 'Explosion',
    'Poison': 'Poison',
    'Trap': 'Trap',
    'Experience': 'Experience',
    'ExperienceOrb': 'ExperienceOrb',
    'ExperienceOrbScript': 'ExperienceOrb',
    'ExperienceMiner': 'ExperienceMiner',
    'VFX': 'VFX',
    'HitDistortion_VFX': 'HitDistortion_VFX',
    'Default': 'Default'
}

replacements = {
    'const char* tagName': 'TagType tagName',
    'if (tag.tag == "Player")': 'if (tag.tag == TagType::Player)',
    'if (tag.tag == "Bullet")': 'if (tag.tag == TagType::Bullet)',
    'if (registry.get<TagComponent>(entity).tag == tagName)': 'if (registry.get<TagComponent>(entity).tag == tagName)', # すでにTagType
}

for f_rel_path in files:
    f_path = normalize_path(os.path.join(r'c:\Users\k024g\source\repos\TD_Engine', f_rel_path))
    if not os.path.exists(f_path):
        print(f"File not found: {f_path}")
        continue
    
    with open(f_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    new_content = content
    
    # 文字列リテラルを置換
    for s_tag, e_tag in tags.items():
        # "Tag" を TagType::Tag に。
        # 単純な文字列置換だが、コンテキストに気をつける
        # == "Tag", != "Tag", = "Tag", ("Tag"), , "Tag"
        new_content = new_content.replace(f'== "{s_tag}"', f'== TagType::{e_tag}')
        new_content = new_content.replace(f'!= "{s_tag}"', f'!= TagType::{e_tag}')
        new_content = new_content.replace(f'= "{s_tag}"', f'= TagType::{e_tag}')
        new_content = new_content.replace(f'("{s_tag}"', f'(TagType::{e_tag}')
        new_content = new_content.replace(f', "{s_tag}"', f', TagType::{e_tag}')

    # 特殊パターンの修正
    new_content = new_content.replace('const char* tagName', 'TagType tagName')
    
    if new_content != content:
        with open(f_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f'Updated: {f_path}')
    else:
        print(f'No changes for: {f_path}')
