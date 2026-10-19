"""English OSD design extension. All layouts are editable vector scenes."""
from scene import *

TOTAL = 10

def chevron(s,x,y,color,back=False):
    d=-1 if back else 1
    s.path([('M',x-3*d,y-5),('L',x+3*d,y),('L',x-3*d,y+5)],stroke=color,sw=1.5)

def category_icon(s,kind,x,y,color):
    if kind=='apps':
        for dx,dy in [(-8,-8),(3,-8),(-8,3),(3,3)]:s.rect(x+dx,y+dy,6,6,color,r=1.5)
    elif kind=='appearance':
        s.circle(x,y,10,stroke=color,sw=1.6)
        with s.group(x,y):s.path(wedge(7,0,-90,90),color)
    else:
        for dy,xx in [(-7,-3),(0,5),(7,-1)]:
            s.line(x-10,y+dy,x+10,y+dy,color,1.5);s.circle(x+xx,y+dy,2.5,color)

def row(s,p,y,label,value,selected=False,chev=True):
    s.rect(24,y,512,47,p['active'] if selected else p['tile'],r=8)
    s.text(40,y+15,label,14,p['text'])
    s.text(504 if chev else 520,y+15,value,13,p['accent'] if selected else p['muted'],align='right')
    if chev:chevron(s,522,y+24,p['muted'])

def osd(s,x,y,theme='dark',page='home',scale=1,language_menu=False):
    p=PALETTES[theme]
    with s.group(x,y,scale):
        shadow_rect(s,0,0,560,360,16,.18)
        s.rect(0,0,560,360,p['surface'],p['line'],1,16)
        if page=='home':
            logo(s,'orbit',24,22,28)
            s.text(64,27,'Copilot Reel',17,p['text'],True)
        else:
            chevron(s,32,36,p['text'],True)
            title={'settings':'Settings','apps':'Apps','editor':'Edit app','appearance':'Appearance'}[page]
            s.text(52,27,title,17,p['text'],True)
        s.line(524,30,534,40,p['muted'],1.4);s.line(534,30,524,40,p['muted'],1.4)
        if page=='home':
            s.text(24,82,'Quick menu',25,p['text'],True)
            data=[('apps','Apps','6 entries'),('appearance','Appearance','Follows Windows'),('settings','Settings','Language & more')]
            for i,(kind,label,detail) in enumerate(data):
                xx=24+i*176
                s.rect(xx,132,160,144,p['active'] if kind=='settings' else p['tile'],p['accent'] if kind=='settings' else p['line'],1,10)
                category_icon(s,kind,xx+28,165,p['accent'] if kind=='settings' else p['text'])
                s.text(xx+16,203,label,16,p['text'],True)
                s.text(xx+16,237,detail,12,p['muted'])
            s.text(24,318,'Soft Orbit',12,p['muted'])
            s.text(536,318,'Esc closes',12,p['muted'],align='right')
        elif page=='settings':
            row(s,p,82,'Language','English',True)
            row(s,p,138,'Start with Windows','Off')
            row(s,p,194,'About','0.1')
            s.text(24,282,'Changes apply immediately.',12,p['muted'])
            s.text(24,321,'Back',12,p['muted'])
            s.text(536,321,'Esc returns',12,p['muted'],align='right')
            if language_menu:
                shadow_rect(s,292,129,244,101,8,.15)
                s.rect(292,129,244,101,p['inner'],p['line'],1,8)
                s.rect(298,135,232,41,p['active'],r=5)
                s.text(316,148,'English',14,p['text'])
                s.path([('M',497,155),('L',502,160),('L',512,149)],stroke=p['accent'],sw=1.6)
                s.text(316,192,'Simplified Chinese',14,p['text'])
        elif page=='apps':
            s.rect(452,20,84,32,p['active'],r=6)
            s.text(494,29,'+ Add app',13,p['accent'],align='center')
            names=['ChatGPT','Claude','Gemini','Codex','Perplexity','Custom']
            for i,name in enumerate(names):
                yy=78+i*38
                s.rect(24,yy,512,34,p['active'] if i==1 else p['tile'],r=6)
                glyph(s,i,43,yy+17,18,p['glyph'])
                s.text(64,yy+9,name,14,p['text'])
                s.text(495,yy+9,'Desktop app' if i in [0,1,3] else 'Website',12,p['muted'],align='right')
                chevron(s,521,yy+17,p['muted'])
            s.text(24,325,'6 of 8 entries',12,p['muted'])
            s.text(536,325,'Reorder',12,p['accent'],align='right')
        elif page=='editor':
            s.text(24,79,'Name',12,p['muted'])
            s.rect(24,101,512,40,p['inner'],p['line'],1,7)
            s.text(38,113,'Claude',14,p['text'])
            s.text(24,158,'Open with',12,p['muted'])
            s.rect(24,180,512,48,p['tile'],p['line'],1,8)
            glyph(s,1,48,204,23,p['glyph'])
            s.text(72,190,'Claude',14,p['text'])
            s.text(72,210,'Installed desktop app',11,p['muted'])
            s.text(518,198,'Change...',13,p['accent'],align='right')
            s.text(26,252,'Advanced',13,p['muted'])
            chevron(s,109,259,p['muted'])
            s.text(24,317,'Remove app',13,p['muted'])
            s.rect(446,302,90,36,p['accent'],r=7)
            s.text(491,313,'Save',14,'#FFFFFF' if theme=='light' else '#102430',True,'center')
        elif page=='appearance':
            row(s,p,82,'Theme','System',chev=False)
            row(s,p,138,'Material','Windows transparency',chev=False)
            row(s,p,194,'Selection color','Windows accent',chev=False)
            s.text(24,270,'Follows Windows. No duplicate theme switches.',12,p['muted'])
            s.rect(24,302,214,34,p['tile'],p['line'],1,6)
            s.text(131,313,'Open Windows appearance',12,p['text'],align='center')

def page(number,tag,title,subtitle=''):
    return base(number,tag,title,subtitle)

def build_pages():
    pages=[]
    s=page(1,'SELECTED DIRECTION / OSD EXTENSION','Soft Orbit. Less interface.','Copilot Reel visual identity v0.3 - native Windows styling, progressive disclosure.')
    wheel(s,'orbit','light',1,350,490,1.2,True)
    osd(s,736,298,'dark','home',1.07)
    s.text(64,742,'Hold to launch. Open the menu to configure.',25,INK,True)
    s.text(64,791,'Vector masters + bitmap previews. OSD screens are design specifications, not implemented screens.',16,MUTED)
    pages.append(s)

    s=page(2,'NAVIGATION MODEL','Two levels. One clear way back.','The primary menu contains entry points. Language and preferences live one level deeper.')
    osd(s,64,265,'light','home',1.03)
    osd(s,790,265,'dark','settings',1.03)
    s.line(672,450,747,450,BLUE,2);chevron(s,747,450,BLUE)
    s.text(64,204,'LEVEL 1 / QUICK MENU',15,BLUE,True)
    s.text(790,204,'LEVEL 2 / SETTINGS',15,BLUE,True)
    note(s,64,683,'ENTRY','A deliberate settings action','Open the quick menu from the tray or a dedicated reel action. A hold gesture never opens an editor.',600)
    note(s,790,683,'RETURN','Back restores context','Back or Esc returns to the primary menu. Closing the OSD preserves the selected app and order.',570)
    pages.append(s)

    s=page(3,'PRIMARY PANEL','A compact control surface.','A monitor OSD-inspired hierarchy, rendered with Windows typography and restrained depth.')
    for x,t in [(64,'light'),(736,'dark')]:
        desktop(s,x,230,640,462,t=='dark');osd(s,x+40,280,t,'home')
        s.text(x,722,'LIGHT' if t=='light' else 'DARK',14,MUTED,True)
    s.text(64,773,'No permanent sidebar, full-window form, or technical launch fields on the home screen.',20,INK)
    pages.append(s)

    s=page(4,'SECONDARY SETTINGS','Preferences belong behind an entry point.','English is the default. Language changes are immediate and remain local to the application.')
    desktop(s,64,223,730,520,True);osd(s,120,290,'dark','settings',1.1)
    note(s,865,235,'LANGUAGE','English / Simplified Chinese','Use one native selection control. Update the OSD, tray, wheel hints, and dialogs together; never rename user-defined apps.',440)
    note(s,865,460,'SCOPE','Small, stable settings','Keep language, startup preference, and version information here. Per-app targets stay in the Apps workflow.',440)
    s.text(64,785,'Startup preference is a proposed OSD control; enabling it requires an explicit user action.',16,MUTED)
    pages.append(s)

    s=page(5,'APPS / TASK SHEET','Choose an app first. Edit only what matters.','App editing uses a focused task sheet; technical options remain collapsed under Advanced.')
    osd(s,64,243,'light','apps',1.1);osd(s,760,243,'dark','editor',1.1)
    s.text(64,683,'APPS',15,BLUE,True)
    s.wrap(64,720,'Show familiar names and original application icons. Add, reorder, and remove are contextual actions.',600,19,30,MUTED)
    s.text(760,683,'APP DETAILS',15,BLUE,True)
    s.wrap(760,720,'Change opens an installed-app or file picker. Arguments, working directory, and AUMID details are advanced fields.',600,19,30,MUTED)
    pages.append(s)

    s=page(6,'SYSTEM APPEARANCE / LANGUAGE','Follow Windows without duplicating its controls.','The material policy and application language are separate decisions.')
    osd(s,64,250,'light','appearance',1.1);osd(s,760,250,'dark','settings',1.1,True)
    s.wrap(64,710,'Light/dark controls the neutral surface. Transparency controls frost versus solid. Accent is reserved for selection.',600,19,30,MUTED)
    s.wrap(760,710,'A small anchored language menu replaces an always-visible configuration field. Selection is persisted independently of app targets.',600,19,30,MUTED)
    pages.append(s)

    s=page(7,'POINTER PLACEMENT / EDGE LAYOUT','The reel follows the pointer. The OSD does not.','The original pointer position is always the cancellation origin. Never silently warp the pointer.')
    for i,(title,detail) in enumerate([('Interior','Full orbit'),('Screen edge','Inward half orbit'),('Screen corner','Inward fan')]):
        x=64+i*448
        s.rect(x,218,416,400,'#F1F5F9',LINE,1,12)
        if i==0:
            wheel(s,'orbit','light',1,x+208,420,.8)
        else:
            origin=(x+394,420) if i==1 else (x+394,594)
            ox,oy=origin
            start,end=(112,248) if i==1 else (196,254)
            with s.group(ox,oy):
                s.path(wedge(240 if i==2 else 190,30,start-10,end+10),'#E7EDF5','#CCD3DC',1)
                for j in range(6):
                    r=(210 if j%2 else 140) if i==2 else 152
                    a=start+(end-start)*j/5
                    gx,gy=point(r,a)
                    s.rect(gx-21,gy-21,42,42,'#FCFCFD','#CCD3DC',1,10)
                    glyph(s,j,gx,gy,25,'#4A515D')
                s.circle(0,0,8,BLUE)
            pointer(s,ox,oy,False,.8)
        s.text(x+24,645,title,22,INK,True)
        s.text(x+24,683,detail,17,MUTED)
    s.wrap(64,743,'Edge layouts are a proposed adaptation: use one shared layout for rendering and hit testing. Preserve item order, expand toward free space, and validate spacing for 1-8 items.',1312,18,28,INK)
    pages.append(s)

    s=page(8,'DESIGN TOKENS / GEOMETRY','One visual system across the reel and OSD.','All sizes are proposal values in DIP. They are not official Windows radial-menu specifications.')
    logo(s,'orbit',64,220,88);s.text(180,237,'Copilot Reel',38,INK,True)
    s.text(64,343,'Segoe UI Variable',30,INK)
    s.text(64,391,'14 DIP body  /  12 DIP hints  /  17 DIP panel title',16,MUTED)
    swatches(s,64,466,[('Surface','#F4F6F8'),('Text','#20252B'),('Accent','#005FB8'),('Selected','#DEECFA')],600)
    s.wrap(64,625,'Use the Windows accent at runtime. Preserve original app icons. The symbols in this study are illustrative placeholders.',590,18,28,MUTED)
    rows=[('Reel diameter','336'),('Cancel diameter','88'),('App icon','40'),('OSD panel','560 × 360'),('Outer radius','16'),('Control radius','6-10'),('Row height','47'),('Content inset','24')]
    for i,(label,value) in enumerate(rows):
        y=222+i*62
        s.text(800,y,label,19,INK);s.text(1350,y,value,21,INK,True,'right');s.line(800,y+41,1350,y+41,LINE)
    pages.append(s)

    s=page(9,'MATERIAL / MOTION / IMPLEMENTATION','Cache content. Animate properties.','Native vector geometry and DirectWrite text remain independent of optional optical effects.')
    wheel(s,'orbit','contrast',1,290,430,.96)
    s.text(290,644,'High-contrast illustration',18,INK,True,'center')
    s.text(290,681,'Use actual Windows system colors.',15,MUTED,align='center')
    note(s,590,222,'MATERIAL POLICY','Respect the user setting','High contrast takes priority. Transparency off uses an opaque neutral surface. Transparency on enables frost only when the platform can supply it.',740)
    note(s,590,414,'TIMING','Short and interruptible','Reel appearance: 100 ms. Selection: 80 ms. OSD page transition: 120 ms maximum. Reduced motion applies the final state immediately.',740)
    note(s,590,606,'RENDERING','No animation image sequence','Reuse geometry, text layouts, and icons. Let the compositor animate opacity and transforms. Stop work while hidden; rebuild only affected resources.',740)
    pages.append(s)

    s=page(10,'HANDOFF / STATUS / REFERENCES','A design extension, with explicit implementation boundaries.','The native reel prototype exists. These OSD screens and edge adaptations are the next UI specification.')
    s.text(64,214,'Implemented prototype',23,INK,True)
    s.wrap(64,258,'Native Soft Orbit rendering; Windows appearance events; app launching and configuration; signed Copilot provider registration. Settings recognition has been verified on the development machine.',610,18,30,MUTED)
    s.text(790,214,'Specified in this revision',23,INK,True)
    s.wrap(790,258,'OSD quick menu; second-level Settings; app task sheets; compact language selection; pointer-edge fan layouts. These drawings do not claim that the new screens are already implemented.',570,18,30,MUTED)
    s.line(64,406,1376,406,LINE)
    refs=[('Windows materials','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials'),('Windows geometry','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry'),('Windows typography','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography'),('Windows motion','learn.microsoft.com/en-us/windows/apps/design/signature-experiences/motion'),('Windows theme support','learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes'),('Apple materials','developer.apple.com/design/human-interface-guidelines/materials')]
    for i,(label,url) in enumerate(refs):
        col=i//3;rowIndex=i%3;xx=64+col*672;yy=443+rowIndex*96
        s.text(xx,yy,label,18,INK,True);s.wrap(xx,yy+32,url,630,13,20,MUTED)
    s.text(64,774,'Review date: 2026-10-19. Native UI supports English and Simplified Chinese; all development specifications use English.',15,MUTED)
    pages.append(s)
    return pages
