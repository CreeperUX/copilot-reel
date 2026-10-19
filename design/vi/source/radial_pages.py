"""Radial OSD specification scenes with unchanged, verified vendor artwork."""
from scene import *

BRANDS = ROOT / 'assets' / 'brands'
MAIN = ['chatgpt', 'claude', 'antigravity', 'zcode', 'vscode', 'jetbrains', 'add', 'settings']
SETTINGS = ['language', 'preferred', 'appearance']
NAMES = {'chatgpt': 'ChatGPT / Codex', 'claude': 'Claude Desktop', 'science': 'Claude for Science',
         'antigravity': 'Antigravity', 'zcode': 'ZCode', 'dsh': 'DeepSeek Harness', 'vscode': 'VS Code',
         'jetbrains': 'JetBrains', 'junie': 'Junie', 'air': 'Air', 'toolbox': 'Toolbox',
         'intellij-idea': 'IntelliJ IDEA', 'add': 'Add / change apps', 'settings': 'Settings',
         'language': 'Language', 'preferred': 'Preferred agent', 'appearance': 'Windows appearance',
         'back': 'Back', 'custom': 'Custom app', 'remove': 'Remove', 'edit': 'Edit details',
         'next': 'More presets', 'previous': 'Previous page'}


def chevron(s, x, y, color, back=False, size=5):
    direction = -1 if back else 1
    s.path([('M', x - size * .5 * direction, y - size),
            ('L', x + size * .5 * direction, y),
            ('L', x - size * .5 * direction, y + size)], stroke=color, sw=1.6)


def mark(s, key, x, y, size, color, theme='light'):
    asset_key = 'claude' if key == 'science' else key
    if asset_key == 'chatgpt': asset_key += '-dark' if theme in ['dark', 'contrast'] else '-light'
    source = BRANDS / (asset_key + '.png')
    if source.exists():
        svg = BRANDS / (asset_key + '.svg')
        s.asset(x-size/2, y-size/2, size, source, svg if svg.exists() else None)
        return
    with s.group(x, y, size / 28):
        if key in ['add', 'custom']:
            s.line(-9, 0, 9, 0, color, 1.75); s.line(0, -9, 0, 9, color, 1.75)
        elif key in ['back', 'previous', 'next']:
            chevron(s, 0, 0, color, key != 'next', 8)
        elif key == 'language':
            s.circle(0, 0, 10, stroke=color, sw=1.75)
            s.path([('M',0,-10),('C',-6,-6,-6,6,0,10),('C',6,6,6,-6,0,-10)], stroke=color, sw=1.5)
            s.line(-9,-3,9,-3,color,1.5);s.line(-9,3,9,3,color,1.5)
        elif key == 'preferred':
            s.circle(0,0,10,stroke=color,sw=1.75)
            s.path([('M',-4,0),('L',-1,4),('L',6,-5)],stroke=color,sw=1.75)
        elif key == 'appearance':
            s.circle(0,0,9,stroke=color,sw=1.75);s.line(0,-9,0,9,color,1.75)
            for angle in range(0,360,45):
                a,b=point(11,angle);c,d=point(14,angle);s.line(a,b,c,d,color,1.5)
        elif key == 'settings':
            s.circle(0,0,7,stroke=color,sw=1.75);s.circle(0,0,2.5,stroke=color,sw=1.75)
            for angle in range(0,360,45):
                a,b=point(7,angle);c,d=point(10.5,angle);s.line(a,b,c,d,color,1.75)
        elif key == 'remove':
            s.line(-8,-7,8,-7,color,1.75);s.line(-3,-10,3,-10,color,1.75)
            s.path([('M',-6,-7),('L',-5,10),('L',5,10),('L',6,-7)],stroke=color,sw=1.75)
        elif key == 'edit':
            s.path([('M',-8,8),('L',-6,2),('L',5,-9),('L',9,-5),('L',-2,6),('Z',)],stroke=color,sw=1.75)
        else:
            glyph(s, 5, 0, 0, 28, color)


def radial(s, x, y, theme='light', items=None, title='Copilot Reel', selected=None,
           scale=1, language=False, preferred=0, guard=False, material='solid', labels=False):
    items = MAIN if items is None else items
    language = language or items == SETTINGS
    p = PALETTES[theme]
    with s.group(x, y, scale):
        if theme != 'contrast': shadow_circle(s, 0, 0, 168, .12)
        s.circle(0,0,168,p['surface'],p['line'],2 if theme=='contrast' else 1,
                 alpha=.9 if material=='frosted' and theme!='contrast' else 1)
        s.circle(0,0,44,stroke=p['line'],sw=1)
        for index,key in enumerate(items):
            ix,iy=point(112,-90+index*360/len(items))
            active=index==selected and not guard
            if active:
                s.rect(ix-30,iy-32,60,64,p['active'],p['accent'],1.5,16)
                s.circle(ix,iy+26,2,p['accent'])
            s.rect(ix-20,iy-20,40,40,p['icon'],r=10)
            mark(s,key,ix,iy,28,p['accent'] if active else p['text'],theme)
            if index==preferred and key not in ['back','settings','language']:
                s.circle(ix+18,iy-18,5.5,p['accent'])
                s.path([('M',ix+15.5,iy-18),('L',ix+17.2,iy-16),('L',ix+20.5,iy-20)],stroke=p['surface'],sw=1.2)
            if labels:
                s.text(ix,iy+38,NAMES.get(key,key),9,p['muted'],align='center')
            elif language and items == SETTINGS:
                label={'language':'Language','preferred':'Preferred','appearance':'Appearance'}[key]
                s.text(ix,iy+36,label,12,p['accent'] if active else p['muted'],align='center')
        if guard:
            heading,hint='Move here','then choose'
        elif language:
            heading,hint='English','A / D to change'
            s.text(0,-37,'Language',12,p['muted'],align='center')
            for xx in [-60,60]:
                s.rect(xx-15,-15,30,30,p['icon'],r=9)
                chevron(s,xx,0,p['text'],xx<0)
        elif selected is not None:
            key=items[selected]
            heading=NAMES.get(key,key)
            if len(heading)>16: heading={'chatgpt':'ChatGPT / Codex','claude':'Claude Desktop','preferred':'Preferred agent','add':'Manage apps'}.get(key,heading)
            hint='Open menu' if key in ['settings','add','language','preferred','appearance'] else 'Choose'
        else:
            heading,hint=title,'Choose an app' if title=='Copilot Reel' else 'Center to go back'
        s.text(0,-16,heading,13 if len(heading)>15 else 14,p['text'],True,'center')
        s.text(0,8,hint,12,p['muted'],align='center')


def page(number, tag, title, subtitle=''):
    return base(number, tag, title, subtitle)


def build_pages():
    pages=[]
    s=page(1,'SOFT ORBIT / RADIAL OSD','Every level stays radial.','Copilot Reel visual identity v0.6 - official app identities, nested wheels, and pointer-led motion.')
    desktop(s,64,219,648,480)
    desktop(s,744,219,632,480,True)
    radial(s,388,455,'light',selected=7,scale=1.1)
    radial(s,1060,455,'dark',items=SETTINGS,title='Settings',selected=0,scale=1.1,preferred=None)
    s.text(64,733,'MAIN REEL',14,BLUE,True);s.text(744,733,'SETTINGS REEL',14,BLUE,True)
    s.text(64,767,'Apps, + management, and Settings in one wheel.',20,INK)
    s.text(744,767,'Same geometry. A deeper level, not another panel.',20,INK)
    pages.append(s)

    s=page(2,'NAVIGATION / SPATIAL MEMORY','One wheel leads to the next.','Keep the center stable while navigating. Back or Esc restores the previous radial level.')
    data=[(240,'Main',MAIN,7),(720,'Settings',SETTINGS,1),(1200,'Preferred agent',MAIN[:6],0)]
    for x,title,items,selected in data:
        radial(s,x,441,'light',items,title,selected,.9,preferred=0 if title!='Settings' else None)
        s.text(x,637,title,22,INK,True,'center')
    for x in [448,928]:
        s.line(x,441,x+58,441,BLUE,2);chevron(s,x+58,441,BLUE)
    note(s,64,704,'MAIN +','Add or change applications','Open the manage reel; select an existing target to edit, or open the preset reel.',600)
    note(s,790,704,'SETTINGS','Language, preferred agent, appearance','Language adjusts in place. The preferred-agent reel defines what a short Copilot tap opens.',570)
    pages.append(s)

    s=page(3,'SECONDARY SETTINGS / IN-PLACE ADJUSTMENT','Language changes inside the wheel.','English is the default. Clicking the two arrows or pressing A / D cycles the native interface language.')
    radial(s,360,456,'dark',SETTINGS,'Settings',0,1.18,language=True,preferred=None)
    s.rect(675,228,701,443,'#F4F7FA',LINE,1,14)
    s.text(715,264,'The adjustment stays local',28,INK,True)
    s.wrap(715,317,'Language is a setting in the second-level wheel. The current value lives in the center with small left/right chevrons. A and D offer the same action.',610,21,34,MUTED)
    s.text(715,466,'English  < >  Simplified Chinese',23,BLUE,True)
    s.wrap(715,522,'Persist immediately. Update wheel hints, tray text, and dialogs together. User-defined app names remain untouched.',610,20,32,MUTED)
    s.text(64,733,'44 x 44 DIP arrow hit regions; 30 x 30 DIP visible buttons.',19,INK)
    s.text(64,775,'After entering settings, the menu stays open for deliberate adjustment. Back returns one level.',17,MUTED)
    pages.append(s)

    s=page(4,'APP MANAGEMENT / PRESETS','Add and edit without leaving the radial hierarchy.','A focused native editor appears only when text or a launch target must be entered.')
    sets=[('Manage apps',['chatgpt','claude','vscode','add'],3),
          ('Agent presets',['chatgpt','claude','science','antigravity','zcode','custom','next'],0),
          ('App actions',['edit','preferred','remove'],0)]
    for i,(title,items,selected) in enumerate(sets):
        x=240+i*480
        radial(s,x,421,'light',items,title,selected,.86,preferred=None)
        s.text(x,610,title,22,INK,True,'center')
    note(s,64,676,'CONFIGURED APPS','Select an app to change it','The + entry opens presets or a custom target. Pagination preserves readable tile size.',600)
    note(s,790,676,'MISSING INSTALLATION','Open a focused editor','Keep the preset identity and ask for the actual app, shortcut, or URI. Do not silently substitute an unrelated website.',570)
    pages.append(s)

    s=page(5,'PRESET IDENTITIES / FIRST COLLECTION','Use the official mark. Unify the surrounding UI.','The initial collection covers the requested agents and the JetBrains family without redrawing their logos.')
    keys=['chatgpt','claude','science','antigravity','zcode','dsh','vscode','jetbrains','junie','air','toolbox','intellij-idea']
    for i,key in enumerate(keys):
        col,row=i%6,i//6;x=64+col*224;y=225+row*236
        s.rect(x,y,192,203,'#F4F6F8',LINE,.8,14)
        s.rect(x+60,y+29,72,72,'#E5E9EF',r=18)
        mark(s,key,x+96,y+65,50,INK)
        s.text(x+96,y+128,NAMES[key],16,INK,True,'center')
        s.text(x+96,y+159,'Verified vendor artwork',11,MUTED,align='center')
    s.wrap(64,733,'Claude for Science shares the Claude identity. The current installed OpenAI package supplies the ChatGPT / Codex mark. DeepSeek Harness is the dsh preset. Resolve actual launch targets before saving.',1312,18,28,MUTED)
    pages.append(s)

    s=page(6,'ICON SYSTEM / OFFICIAL ARTWORK','Consistency comes from the container.','Keep vendor proportions and recognizable color. Apply common optical bounds, spacing, selection, and focus.')
    for i,theme in enumerate(['light','dark','contrast']):
        p=PALETTES[theme];x=64+i*448
        s.rect(x,226,416,288,p['surface'],p['line'],1,14)
        for j,key in enumerate(['chatgpt','claude','vscode']):
            xx=x+78+j*130
            s.rect(xx-37,314,74,82,p['active'] if j==1 else p['surface'],p['accent'] if j==1 else None,2,18)
            s.rect(xx-28,326,56,56,p['icon'],r=14)
            mark(s,key,xx,354,39,p['text'],theme)
        s.text(x+24,458,theme.upper(),16,p['text'],True)
    note(s,64,569,'GEOMETRY','40 DIP tile / 28 DIP mark','Use a 10 DIP tile radius and 60 x 64 DIP selected seat. Do not recolor, distort, crop, or replace the official mark to force consistency.',600)
    note(s,790,569,'NATIVE RENDERING','Vector UI with source-aware assets','Direct2D renders supported SVGs. Official PNGs cover sources without usable vector artwork or unsupported SVG masks. These are identity assets, not UI frames.',570)
    s.text(64,778,'Source ownership, URLs, processing, and SHA-256 checksums: assets/brands/provenance.json.',16,MUTED)
    pages.append(s)

    s=page(7,'POINTER ORIGIN / EDGE AND CORNER','The pointer starts the gesture. The whole reel stays visible.','The wheel is placed near the actual pointer on its monitor; it is never opened at a fixed screen center.')
    for i,title in enumerate(['Interior','Screen edge','Screen corner']):
        x=64+i*448;s.rect(x,221,416,397,'#ECF2F8',LINE,1,12)
        s.text(x+18,239,'MONITOR WORK AREA',11,MUTED)
        cx=x+208;cy=421
        ax,ay=(cx,cy+134) if i==0 else (x+394,cy) if i==1 else (x+394,595)
        s.line(ax,ay,cx,cy,BLUE,1.8)
        radial(s,cx,cy,'light',selected=None,scale=.8,guard=True,preferred=0)
        pointer(s,ax,ay,False,.8)
        s.text(x+24,644,title,22,INK,True)
        s.wrap(x+24,685,'Nominal tether: 168 DIP.' if i==0 else 'Clamp the full footprint; tether may be longer.',365,18,28,MUTED)
    s.wrap(64,758,'Never warp the system pointer. If it starts outside the final center zone, wait for center entry before allowing release-to-launch. The visible center is the only armed hit-test origin.',1312,18,28,INK)
    pages.append(s)

    s=page(8,'MOTION / POINTER-LED REVEAL','Extend. Sweep. Settle. Reverse to close.','Windows Composition evaluates the timeline. Icons stay upright and the application does not paint every frame.')
    for i,(title,stamp) in enumerate([('Origin','0 ms'),('Tether reaches center','70 ms'),('Circular reveal','70-230 ms'),('Ready to select','230 ms')]):
        x=64+i*336;s.rect(x,227,304,360,'#F2F6FA',LINE,1,12)
        cx=x+152;cy=407;ax=cx;ay=cy+108
        if i==0:s.circle(ax,ay,3,BLUE)
        else:s.line(ax,ay,cx,cy,BLUE,2)
        if i==2:
            s.circle(cx,cy,61,'#F4F6F8',LINE,1)
            with s.group(cx,cy):s.path(arc(107,-90,90),stroke=BLUE,sw=2)
        if i==3:radial(s,cx,cy,'light',scale=.64,guard=True)
        pointer(s,ax,ay,False,.72)
        s.text(x+18,619,title,18,INK,True)
        s.text(x+18,653,stamp,16,BLUE)
    note(s,64,709,'OPEN / 230 MS','Tether 70 ms + reveal 160 ms','Clip radius, opacity, scale, and one full outline sweep run on the compositor.',600)
    note(s,790,709,'CLOSE / 190 MS','Reverse, then detach','Reverse the reveal and retract the line. A new gesture interrupts closing; reduced motion is immediate.',570)
    pages.append(s)

    s=page(9,'SYSTEM APPEARANCE / MATERIALS','Follow the Windows surface and effect preferences.','Neutral light/dark bases; system accent for selection. Frost is optional and never replaces readable content.')
    for i,(theme,material) in enumerate([('light','solid'),('light','frosted'),('dark','solid'),('dark','frosted')]):
        x=64+i*336;p=PALETTES[theme]
        desktop(s,x,225,304,390,theme=='dark')
        radial(s,x+152,424,theme,selected=7,scale=.72,material=material)
        s.text(x+152,645,theme.title()+' / '+material.title(),20,INK,True,'center')
    s.wrap(64,713,'Transparency off: opaque neutral base with no backdrop sampling. Transparency on: a frosted backdrop when available. High contrast, effects capability, and energy saver take precedence. Reduced motion skips decorative transitions.',1312,20,31,INK)
    s.text(64,795,'Frost previews illustrate tone only. Actual host-backdrop blur requires native runtime validation.',15,MUTED)
    pages.append(s)

    s=page(10,'GEOMETRY / INPUT CONTRACT','Stable sizes. Different intent for tap and hold.','Native vector geometry is shared by layout, drawing, and hit testing at the active monitor DPI.')
    rows=[('Reel diameter / icon orbit','336 / 112 DIP'),('Center cancellation zone','88 DIP diameter'),('Tile / artwork / corner','40 / 28 / 10 DIP'),('Selection seat','60 x 64 DIP'),('Language arrow hit / visible','44 x 44 / 30 x 30 DIP'),('Body / hints','14 / 12 DIP'),('Normal pointer-to-center distance','168 DIP'),('Entry / exit timeline','230 / 190 ms')]
    for i,(label,value) in enumerate(rows):
        y=223+i*66;s.text(64,y,label,20,INK);s.text(725,y,value,20,INK,True,'right');s.line(64,y+42,725,y+42,LINE)
    note(s,830,224,'COPILOT SHORT TAP','Open the preferred agent','A short tap launches the saved preferred target directly. It does not open a wheel preview.',510)
    note(s,830,410,'COPILOT HOLD','Open the pointer-led reel','Windows supplies the hold threshold, approximately 300 ms. Release opens one selected agent or enters the chosen radial menu.',510)
    note(s,830,613,'SETTINGS CONTEXT','Stay open to adjust','Secondary menus accept deliberate clicks and A / D language changes. Back or Esc returns one level.',510)
    pages.append(s)

    s=page(11,'PERFORMANCE / ACCESSIBILITY / LIMITS','No fixed frame budget hidden in the design.','Cached content and compositor timelines adapt to presentation cadence; measured behavior remains the acceptance test.')
    note(s,64,222,'INPUT','Selection owns the action','Each release resolves at most once. Animation cannot change the selected target or repeat launch. Raw pointer traffic is coalesced into state changes.',600)
    note(s,790,222,'DRAWING','Static content stays cached','Redraw for selection, menu, theme, DPI, or device changes. Decode official assets before the gesture. No permanent 60 Hz application loop.',570)
    note(s,64,442,'ACCESS','Respect Windows preferences','High contrast uses system surfaces and clear target outlines. Disabled animations skip motion. Text uses Segoe UI Variable with system fallback.',600)
    note(s,790,442,'DISPLAY','Keep validation separate','Test 60 / 144 / 240 Hz, mixed DPI, multiple monitors, energy saver, hidden wakeups, and rapid interruption. Renderer API success is not a performance result.',570)
    s.rect(64,673,1312,119,'#F1F6FB',r=12)
    s.text(88,694,'Small work areas and accessibility',22,INK,True)
    s.wrap(88,732,'If the complete canvas cannot fit, scale the whole circle uniformly instead of opening a different dashboard. Very small work areas reduce text and targets; accessibility and usability at those sizes require explicit validation.',1264,17,27,MUTED)
    pages.append(s)

    s=page(12,'HANDOFF / REFERENCES / EVIDENCE','A radial product surface, with explicit source boundaries.','The v0.6 specification replaces the rectangular primary OSD. Focused native editors remain for text entry only.')
    note(s,64,220,'IMPLEMENTATION CONTRACT','Native wheel and nested navigation','Main: configured agents, + management, Settings. Settings: language adjustment, preferred agent, appearance. Manage: existing targets and preset collection. Missing targets open the editor.',610)
    note(s,790,220,'ASSET CONTRACT','Official identity, common treatment','Keep source artwork and ownership records. Preserve vendor proportions. A neutral application glyph handles missing assets; do not invent brand approximations.',570)
    s.line(64,447,1376,447,LINE)
    refs=[('Windows motion','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/motion'),('Composition animation','learn.microsoft.com/en-us/windows/uwp/composition/composition-animation'),('Windows materials','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials'),('Radial precedent: Kando','kando.menu/config-files/#warpmouse'),('Radial precedent: Blender','docs.blender.org/manual/en/latest/editors/preferences/interface.html'),('Asset provenance','assets/brands/provenance.json')]
    for i,(name,url) in enumerate(refs):
        x=64+(i//3)*672;y=476+(i%3)*91
        s.text(x,y,name,18,INK,True);s.wrap(x,y+30,url,620,13,20,MUTED)
    s.text(64,789,'Specification date: 2026-10-19. Runtime test results and remaining limits belong in docs/validation.md.',15,MUTED)
    pages.append(s)
    return pages
