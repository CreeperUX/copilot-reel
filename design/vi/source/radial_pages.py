"""Quiet radial UI specification using original vendor artwork."""
from scene import *

BRANDS = ROOT / 'assets' / 'brands'
MAIN = ['chatgpt', 'claude', 'antigravity', 'zcode', 'vscode', 'jetbrains', 'add']
SETTINGS = ['language', 'appearance', 'preferred', 'manage', 'add']
NAMES = {'chatgpt': 'ChatGPT / Codex', 'claude': 'Claude Desktop', 'science': 'Claude for Science',
         'antigravity': 'Antigravity', 'zcode': 'ZCode', 'dsh': 'DeepSeek Harness', 'vscode': 'VS Code',
         'gemini': 'Gemini', 'perplexity': 'Perplexity',
         'jetbrains': 'JetBrains', 'junie': 'Junie', 'air': 'Air', 'toolbox': 'Toolbox',
         'intellij-idea': 'IntelliJ IDEA', 'add': 'Add app', 'settings': 'Settings', 'manage': 'Manage apps',
         'language': 'Language', 'preferred': 'Preferred agent', 'appearance': 'Appearance',
         'back': 'Back', 'custom': 'Custom app', 'remove': 'Remove', 'edit': 'Edit details',
         'next': 'More presets', 'previous': 'Previous page'}
COMMANDS = {'add','settings','manage','language','preferred','appearance','back','custom','remove','edit','next','previous'}


def chevron(s, x, y, color, back=False, size=5):
    direction = -1 if back else 1
    s.path([('M', x - size * .5 * direction, y - size),
            ('L', x + size * .5 * direction, y),
            ('L', x - size * .5 * direction, y + size)], stroke=color, sw=1.6)


def mark(s, key, x, y, size, color, theme='light', language_code='EN'):
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
            s.text(0,-8.2,language_code,16,color,True,'center')
        elif key == 'preferred':
            s.circle(0,0,10,stroke=color,sw=1.75)
            s.path([('M',-4,0),('L',-1,4),('L',6,-5)],stroke=color,sw=1.75)
        elif key == 'appearance':
            if theme in ['dark','contrast']:
                s.path([('M',4,-10),('C',-7,-11,-14,2,-6,9),('C',1,15,11,8,11,2),
                        ('C',4,8,-4,-3,4,-10),('Z',)],stroke=color,sw=1.75)
            else:
                s.circle(0,0,6,stroke=color,sw=1.75)
                for angle in range(0,360,45):
                    a,b=point(9,angle);c,d=point(12,angle);s.line(a,b,c,d,color,1.5)
        elif key == 'manage':
            for xx in [-6,6]:
                for yy in [-6,6]:s.rect(xx-3.2,yy-3.2,6.4,6.4,stroke=color,sw=1.65,r=1.5)
        elif key == 'remove':
            s.line(-8,-7,8,-7,color,1.75);s.line(-3,-10,3,-10,color,1.75)
            s.path([('M',-6,-7),('L',-5,10),('L',5,10),('L',6,-7)],stroke=color,sw=1.75)
        elif key == 'edit':
            s.path([('M',-8,8),('L',-6,2),('L',5,-9),('L',9,-5),('L',-2,6),('Z',)],stroke=color,sw=1.75)
        else:glyph(s, 5, 0, 0, 28, color)


def radial(s, x, y, theme='light', items=None, title='Copilot Reel', selected=None,
           scale=1, language=False, preferred=0, guard=False, material='solid', language_code='EN'):
    items = MAIN if items is None else items
    language = language or items == SETTINGS
    submenu = title != 'Copilot Reel'
    p = PALETTES[theme]
    with s.group(x, y, scale):
        if theme != 'contrast': shadow_circle(s, 0, 0, 168, .10)
        s.circle(0,0,168,p['surface'],alpha=.9 if material=='frosted' and theme!='contrast' else 1)
        s.circle(0,0,168,stroke=p['line'],sw=2 if theme=='contrast' else 1,alpha=1 if theme=='contrast' else .45)
        # A neutral center affordance stays spatially stable without explanatory copy.
        s.circle(0,0,44,stroke=p['line'],sw=1,alpha=.55 if guard else .18)
        for index,key in enumerate(items):
            ix,iy=point(112,-90+index*360/len(items))
            active=index==selected and not guard
            if active:
                s.rect(ix-30,iy-32,60,64,p['active'],p['accent'],1.5,16)
                s.circle(ix,iy+26,2,p['accent'])
            s.rect(ix-20,iy-20,40,40,p['icon'],r=10)
            mark(s,key,ix,iy,28,p['accent'] if active else p['text'],theme,language_code)
            if index==preferred and key not in COMMANDS:
                s.circle(ix+18,iy-18,5.5,p['accent'])
                s.path([('M',ix+15.5,iy-18),('L',ix+17.2,iy-16),('L',ix+20.5,iy-20)],stroke=p['surface'],sw=1.2)
        if language:
            # Arrows flank the EN/ZH tile rather than occupying the center.
            for xx in [-48,48]:
                chevron(s,xx,-112,p['text'],xx<0,5)
        if not guard and selected is not None:
            key=items[selected];label='Apps and settings' if not submenu and key=='add' else NAMES.get(key,key)
            s.text(0,-7,label,14,p['text'],True,'center')
        elif submenu:chevron(s,0,0,p['muted'],True,6)
        elif guard:s.circle(0,0,3,p['muted'],alpha=.7)


def page(number, tag, title, subtitle=''):
    return base(number, tag, title, subtitle)


def build_pages():
    pages=[]
    s=page(1,'SOFT ORBIT / QUIET RADIAL UI','Less copy. A clearer point of origin.','Copilot Reel visual identity v0.8 - one + entry, stateful icons, and restrained native motion.')
    desktop(s,64,219,648,480)
    desktop(s,744,219,632,480,True)
    radial(s,388,455,'light',scale=1.1)
    radial(s,1060,455,'dark',items=SETTINGS,title='Settings',scale=1.1,preferred=None)
    s.text(64,733,'MAIN REEL',14,BLUE,True);s.text(744,733,'UNIFIED + HUB',14,BLUE,True)
    s.text(64,767,'Agents and one +. The center stays quiet.',20,INK)
    s.text(744,767,'Add, manage, language, appearance, preference.',20,INK)
    pages.append(s)

    s=page(2,'NAVIGATION / ONE ENTRY POINT','One + combines adding and settings.','Keep the center stable through navigation. A center chevron or Esc returns one radial level.')
    data=[(240,'Main',MAIN,6),(720,'Unified hub',SETTINGS,None),(1200,'Preferred agent',MAIN[:6],0)]
    for x,title,items,selected in data:
        radial(s,x,441,'light',items,'Copilot Reel' if title=='Main' else title,selected,.9,preferred=0 if title!='Unified hub' else None)
        s.text(x,637,title,22,INK,True,'center')
    for x in [448,928]:
        s.line(x,441,x+58,441,MUTED,1.5);chevron(s,x+58,441,MUTED)
    note(s,64,704,'PRIMARY','One structural command','Main contains configured agents and a single +. The separate Settings gear is removed.',600)
    note(s,790,704,'SECONDARY','Five clear icon actions','EN/ZH, appearance, preferred agent, app management, and adding. Text entry still uses a focused native editor.',570)
    pages.append(s)

    s=page(3,'STATEFUL ICONS / LANGUAGE','The language is the icon.','EN and ZH identify the active interface language. Small chevrons and A / D switch it in place.')
    radial(s,360,456,'dark',SETTINGS,'Settings',0,1.18,language=True,preferred=None)
    s.rect(675,228,701,443,'#F4F7FA',LINE,1,14)
    for i,code in enumerate(['EN','ZH']):
        x=815+i*286;s.rect(x-44,294,88,88,'#E5E9EF',r=22)
        mark(s,'language',x,338,60,INK,language_code=code)
        s.text(x,410,'English' if code=='EN' else 'Simplified Chinese',20,INK,True,'center')
    s.wrap(715,493,'Hover the tile or either arrow to see Language at the center. EN/ZH still conveys the active value. The label is one line, with no explanation beneath it; the idle hub returns to its back chevron.',610,20,32,MUTED)
    s.text(64,733,'40 x 40 DIP arrow hit regions; small unfilled chevrons.',19,INK)
    s.text(64,775,'The icon changes immediately after a successful save. User-defined app names stay untouched.',17,MUTED)
    pages.append(s)

    s=page(4,'APP MANAGEMENT / PRESETS','Adding and editing stay in the same family.','The hub branches into existing apps or presets. A native editor appears only for text and launch details.')
    sets=[('Manage apps',['chatgpt','claude','vscode','add'],None),
          ('Agent presets',['chatgpt','claude','science','antigravity','zcode','custom','next'],0),
          ('App actions',['edit','preferred','remove'],None)]
    for i,(title,items,selected) in enumerate(sets):
        x=240+i*480
        radial(s,x,421,'light',items,title,selected,.86,preferred=None)
        s.text(x,610,title,22,INK,True,'center')
    note(s,64,676,'HOVER RECOGNITION','Name the item under the pointer','A selected application or secondary command shows one concise functional name at the center. Idle Main stays empty; submenu idle state keeps only the back chevron.',600)
    note(s,790,676,'MISSING INSTALLATION','Keep the intended identity','Missing presets use the focused editor. A missing short-tap target uses a native executable/shortcut picker. Neither flow substitutes a website.',570)
    pages.append(s)

    s=page(5,'PRESET IDENTITIES / FIRST COLLECTION','Official marks, in a common visual container.','Preserve vendor silhouettes, proportions, and recognizable color. The launcher supplies a consistent frame.')
    keys=['chatgpt','claude','science','antigravity','zcode','dsh','vscode','gemini','perplexity','jetbrains','junie','intellij-idea']
    for i,key in enumerate(keys):
        col,row=i%6,i//6;x=64+col*224;y=225+row*236
        s.rect(x,y,192,203,'#F4F6F8',LINE,.8,14)
        s.rect(x+60,y+29,72,72,'#E5E9EF',r=18)
        mark(s,key,x+96,y+65,50,INK)
        s.text(x+96,y+128,NAMES[key],16,INK,True,'center')
        s.text(x+96,y+159,'Verified vendor artwork',11,MUTED,align='center')
    s.wrap(64,733,'Gemini and Perplexity retain their official artwork. Windows Settings and Notepad use recognized native system glyphs. Claude for Science shares Claude artwork; resolve actual launch targets before saving.',1312,18,28,MUTED)
    pages.append(s)

    s=page(6,'ICON SYSTEM / HOVER RECOGNITION','A name appears only when it helps.','Keep idle centers quiet. Hovering a command reveals its function without a fixed caption or second line.')
    for i,(theme,index,title) in enumerate([('light',1,'Appearance'),('dark',3,'Manage apps'),('contrast',2,'Preferred agent')]):
        x=240+i*480
        radial(s,x,425,theme,SETTINGS,'Settings',index,.88,preferred=None)
        s.text(x,621,title,21,INK,True,'center')
    note(s,64,688,'IDENTITY','One functional name','Use the same name for pointer and keyboard selection. Hovering the language arrows selects Language. Idle submenus return to their small back chevron.',600)
    note(s,790,688,'TYPE','14 DIP; one line','Keep the selected name centered within 132 DIP. Ellipsis handles unusually long app names. Do not add an instruction or subtitle.',570)
    pages.append(s)

    s=page(7,'POINTER ORIGIN / EDGE AND CORNER','Inside the work area, the pointer is the center.','Only an actual boundary conflict moves the circle. There is no nominal offset and no opening connector.')
    for i,title in enumerate(['Interior','Screen edge','Screen corner']):
        x=64+i*448;s.rect(x,221,416,397,'#ECF2F8',LINE,1,12)
        s.text(x+18,239,'MONITOR WORK AREA',11,MUTED)
        cx=x+208 if i==0 else x+416-153.6
        cy=618-153.6 if i==2 else 421
        ax,ay=(cx,cy) if i==0 else (x+414,cy) if i==1 else (x+414,616)
        radial(s,cx,cy,'light',scale=.8,guard=i!=0,preferred=0)
        pointer(s,ax,ay,False,.8)
        s.text(x+24,644,title,22,INK,True)
        s.wrap(x+24,685,'Zero offset. Choose a direction immediately.' if i==0 else 'Move only as far inward as the footprint requires.',365,18,28,MUTED)
    s.wrap(64,758,'Never warp the pointer. If edge clamping leaves it outside the final center zone, keep selection unarmed until center entry. The interior path needs no extra center-entry movement.',1312,18,28,INK)
    pages.append(s)

    s=page(8,'MOTION / QUIET ARRIVAL','A short dissolve and a small change in scale.','The circle appears at its final location. No extending line, perimeter sweep, or rotating artwork competes with selection.')
    for i,(title,stamp,scale) in enumerate([('Initial','0 ms',.96),('Arriving','80 ms',.99),('Ready','160 ms',1),('Closing','60 ms',.98)]):
        x=64+i*336;s.rect(x,227,304,360,'#F2F6FA',LINE,1,12)
        cx=x+152;cy=407
        radial(s,cx,cy,'light',scale=.64*scale,preferred=None)
        if i in [0,1,3]:s.rect(x+2,229,300,356,'#F2F6FA',r=10,alpha={0:1.0,1:.35,3:.65}[i])
        pointer(s,cx,cy,False,.72)
        s.text(x+18,619,title,18,INK,True)
        s.text(x+18,653,stamp,16,MUTED)
    note(s,64,709,'OPEN / 160 MS','Opacity 0 to 1; scale 0.96 to 1','Ease into the final state without overshoot. The pointer remains at the center in the normal interior case.',600)
    note(s,790,709,'CLOSE / 120 MS','Reverse, then detach','Reverse opacity and scale. Interruption starts from the current presentation value; reduced motion is immediate.',570)
    pages.append(s)

    s=page(9,'SYSTEM APPEARANCE / MATERIALS','Let Windows determine the base surface.','Neutral light/dark bases, with a subtle boundary. Frost is optional and never replaces readable content.')
    for i,(theme,material) in enumerate([('light','solid'),('light','frosted'),('dark','solid'),('dark','frosted')]):
        x=64+i*336
        desktop(s,x,225,304,390,theme=='dark')
        radial(s,x+152,424,theme,scale=.72,material=material)
        s.text(x+152,645,theme.title()+' / '+material.title(),20,INK,True,'center')
    s.wrap(64,713,'Transparency off: opaque neutral base with no backdrop sampling. Transparency on: a frosted backdrop when available. High contrast, effects capability, and energy saver take precedence. Reduced motion skips the transition.',1312,20,31,INK)
    s.text(64,795,'Frost previews illustrate tone only. Actual host-backdrop blur requires native runtime validation.',15,MUTED)
    pages.append(s)

    s=page(10,'GEOMETRY / INPUT CONTRACT','Stable sizes, anchored to the actual pointer.','Native vector geometry is shared by layout, drawing, and hit testing at the active monitor DPI.')
    rows=[('Reel diameter / icon orbit','336 / 112 DIP'),('Center cancellation zone','88 DIP diameter'),('Tile / artwork / corner','40 / 28 / 10 DIP'),('Selection seat','60 x 64 DIP'),('Language arrow hit / glyph','40 x 40 / 5 x 10 DIP'),('Hover name / icon monogram','14 / 16 DIP'),('Interior pointer-to-center offset','0 DIP'),('Entry / exit timeline','160 / 120 ms')]
    for i,(label,value) in enumerate(rows):
        y=223+i*66;s.text(64,y,label,20,INK);s.text(725,y,value,20,INK,True,'right');s.line(64,y+42,725,y+42,LINE)
    note(s,830,224,'COPILOT SHORT TAP','Open the local application','Use the preferred installed app. If unavailable, a native picker requests an executable or shortcut. Never open a website as the tap fallback.',510)
    note(s,830,410,'COPILOT HOLD','Open at the pointer','Windows supplies the approximately 300 ms threshold. Move outward to select; release opens once or enters the hub.',510)
    note(s,830,613,'HUB CONTEXT','Stay open to adjust','The + hub accepts deliberate clicks and A / D language changes. Center or Esc returns one level.',510)
    pages.append(s)

    s=page(11,'PERFORMANCE / ACCESSIBILITY / LIMITS','The visual layer remains event driven.','Cached content and compositor timelines follow presentation cadence; measured behavior remains the acceptance test.')
    note(s,64,222,'INPUT','Selection owns the action','Each release resolves at most once. Animation cannot change the selected target or repeat a launch. Pointer traffic is reduced to meaningful state changes.',600)
    note(s,790,222,'DRAWING','Static content stays cached','Redraw for selection, menu, theme, DPI, or device changes. Decode official assets before the gesture. No permanent 60 Hz application loop.',570)
    note(s,64,442,'ACCESS','Respect Windows preferences','High contrast uses system surfaces and clear target outlines. Disabled animations skip motion. Semantic action names remain in the model even when visual captions are absent.',600)
    note(s,790,442,'DISPLAY','Validate the presentation','Test 60 / 144 / 240 Hz, mixed DPI, multiple monitors, energy saver, hidden wakeups, and rapid interruption. Renderer API success is not a performance result.',570)
    s.rect(64,673,1312,119,'#F1F6FB',r=12)
    s.text(88,694,'Small work areas and accessibility',22,INK,True)
    s.wrap(88,732,'If the complete canvas cannot fit, scale the whole circle uniformly. Very small work areas reduce text and targets; accessibility and icon discoverability require explicit validation.',1264,17,27,MUTED)
    pages.append(s)

    s=page(12,'HANDOFF / REFERENCES / EVIDENCE','Quiet structure, explicit implementation rules.','The v0.8 specification refines the native radial product. Focused native editors remain for text entry only.')
    note(s,64,220,'IMPLEMENTATION CONTRACT','One + hub; hover names','Main: configured agents and +. Hub: language, appearance, preferred agent, management, and adding. Idle centers stay quiet; a hovered action gets one concise functional name.',610)
    note(s,790,220,'LOCAL LAUNCH CONTRACT','Resolve the application first','Defaults discover installed apps. A missing short-tap target opens the native application picker. Only known base-URL defaults migrate, with a backup; custom URLs remain deliberate wheel actions.',570)
    s.line(64,447,1376,447,LINE)
    refs=[('Windows motion','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/motion'),('Composition animation','learn.microsoft.com/en-us/windows/uwp/composition/composition-animation'),('Windows materials','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials'),('Radial precedent: Kando','kando.menu/config-files/#warpmouse'),('Radial precedent: Blender','docs.blender.org/manual/en/latest/editors/preferences/interface.html'),('Asset provenance','assets/brands/provenance.json')]
    for i,(name,url) in enumerate(refs):
        x=64+(i//3)*672;y=476+(i%3)*91
        s.text(x,y,name,18,INK,True);s.wrap(x,y+30,url,620,13,20,MUTED)
    s.text(64,789,'Specification date: 2026-10-20. Runtime evidence and remaining limits belong in docs/validation.md.',15,MUTED)
    pages.append(s)
    return pages
