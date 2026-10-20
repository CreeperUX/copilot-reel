"""Generate the English radial VI PDF, editable SVG scenes, and semantic tokens."""
from scene import *
from radial_pages import build_pages, radial, MAIN, SETTINGS


def main():
    VECTORS.mkdir(parents=True, exist_ok=True)
    PDF_DIR.mkdir(parents=True, exist_ok=True)
    pages = build_pages()
    path = PDF_DIR / 'copilot-reel-vi-v0.8.pdf'
    document = canvas.Canvas(str(path), pagesize=(PAGE_W*PDF_SCALE, PAGE_H*PDF_SCALE), pageCompression=1)
    document.setTitle('Copilot Reel - Quiet Radial UI and Official App Identity v0.8')
    document.setAuthor('Copilot Reel')
    for page in pages:
        page.draw_pdf(document)
        document.showPage()
    document.save()

    # Retire the rejected rectangular primary-menu masters from the active set.
    for stale in VECTORS.glob('osd-*.svg'):
        stale.unlink()
    for theme in ['light', 'dark', 'contrast']:
        scene = Scene(420, 420)
        radial(scene, 210, 210, theme, selected=6)
        (VECTORS / f'orbit-{theme}.svg').write_text(scene.svg('Soft Orbit / radial main / '+theme), encoding='utf-8', newline='\n')
        if theme == 'contrast': continue
        for material in ['solid', 'frosted']:
            scene=Scene(420,420)
            radial(scene,210,210,theme,selected=6,material=material)
            (VECTORS/f'orbit-{theme}-{material}.svg').write_text(scene.svg('Soft Orbit / '+theme+' / '+material),encoding='utf-8',newline='\n')
        screens=[('main',MAIN,'Copilot Reel',6,False),
                 ('settings',SETTINGS,'Settings',0,False),
                 ('language',SETTINGS,'Settings',0,True),
                 ('preferred',MAIN[:6],'Preferred agent',0,False),
                 ('manage',['chatgpt','claude','vscode','add'],'Manage apps',3,False),
                 ('presets',['chatgpt','claude','science','antigravity','zcode','custom','next'],'Agent presets',0,False)]
        for name,items,title,selected,language in screens:
            scene=Scene(420,420)
            radial(scene,210,210,theme,items,title,selected,language=language,preferred=0 if name in ['main','preferred'] else None)
            (VECTORS/f'radial-{name}-{theme}.svg').write_text(scene.svg('Radial OSD / '+name+' / '+theme),encoding='utf-8',newline='\n')

    tokens={
        'schemaVersion':'0.8','status':'native-implementation-with-runtime-validation-tracked-separately',
        'units':'DIP','defaultScheme':'orbit','defaultLanguage':'en-US','supportedLanguages':['en-US','zh-CN'],
        'rendering':'native vector geometry and DirectWrite text; official artwork uses source-appropriate SVG or PNG',
        'typography':{'preferred':'Segoe UI Variable','fallback':'Segoe UI','cjk':'system fallback','applicationLabel':14,'iconMonogram':16,'helperText':False},
        'interaction':{'systemHoldThresholdMs':300,'singleTap':'launch preferred local application; resolve recognized legacy website defaults through installed-app discovery; open native executable/shortcut picker when unavailable; never fall back to a website','hold':'open at pointer center; release commits a target or opens the unified + hub','centerCancelDiameter':88,'releaseCommitsAtMostOnce':True,'gapsAreNotDeadZones':True,'menuContinuation':'secondary settings remain open for deliberate adjustment','back':'center or Esc returns one level'},
        'motion':{'appearanceMs':160,'exitMs':120,'selectionMs':0,'menuPageMs':0,'appearanceScale':[.96,1.0],'outlineSweepDegrees':0,'connector':False,'iconsRotate':False,'reduceMotion':'instant state changes','implementation':'Windows Composition opacity and scale; one-shot exit completion only','hidden':'detach composition tree; stop active animations'},
        'colorPolicy':{'surface':'Windows app light/dark neutral theme','accent':'Windows accent for selection outlines and indicators','vendorArtwork':'preserve official proportions and recognizable colors','sampleLight':'#005FB8','sampleDark':'#60CDFF','highContrast':'actual system surfaces and readable outlines; preview colors illustrative'},
        'systemAppearance':{'followSystem':True,'materialPriority':['high-contrast-system-solid','transparency-disabled-or-unavailable-theme-solid','transparency-enabled-and-supported-frosted'],'solid':{'surfaceAlpha':1.0,'backgroundSampling':False,'backgroundBlur':False},'frosted':{'surface':'single compositor backdrop layer','preview':'tonal illustration; native backdrop requires runtime validation'},'delivery':'coalesce on UI thread; preserve menu context and selection'},
        'palettes':PALETTES,
        'schemes':{'orbit':{'name':'Soft Orbit','status':'selected','diameter':336,'icon':40,'orbitRadius':112,'selectionSeat':[60,64]},'halo':{'status':'historical comparison'},'petals':{'status':'historical comparison'}},
        'iconography':{'renderer':'verified vendor artwork plus native command vectors','tileSize':40,'tileRadius':10,'artworkSize':28,'commandStrokeWidth':1.75,'highContrastCommandStrokeWidth':2,'identity':'preserve official artwork and use consistent container geometry','unknown':'neutral application-window symbol','initialsFallback':False,'sourceManifest':'assets/brands/provenance.json','rasterException':'official PNG or same-artwork PNG when SVG unavailable or unsupported; never UI animation frames'},
        'osd':{'surface':'nested radial menus','primary':['configured agents','+'],'settings':['Language','Appearance','Preferred agent','Manage apps','Add app'],'language':{'location':'unified + hub / language tile','input':['left chevron','right chevron','A','D'],'iconStates':['EN','ZH'],'arrowHitSize':[40,40],'arrowVisibleSize':[5,10],'arrowBackground':False,'arrowCentersFromCenter':[[-48,-112],[48,-112]],'default':'English','persistence':'immediate'},'management':['existing apps','presets with five items per page','custom app','previous/next when available'],'editor':'focused native task sheet for name and actual launch target','missingPreset':'open focused local-target editor; do not substitute a website','defaultTargets':'up to six installed native applications, then Notepad and Windows Settings','legacyMigration':'known ChatGPT and Claude base-URL defaults only; back up configuration before automatic save; preserve arbitrary/custom URLs','missingPreferredTarget':'native Choose a local application picker restricted to exe/lnk; replace selected launch profile before opening','customWebTargets':'explicit wheel selection only; never a short-tap fallback','autostartSetting':False,'primaryRectangularPanel':False,'centerContent':'empty on idle Main; back chevron on idle submenus; one concise functional name for the selected app or secondary command','centerLabelPolicy':'show the hovered item name, including Language, Appearance, Preferred agent, Manage apps, and Add app; never add a second line or helper prompt','captions':False,'appearanceIcon':'sun for system light mode; moon for system dark mode'},
        'edgeLayout':{'interior':'center the complete reel on the actual pointer with zero offset','pointerOffset':0,'edge':'clamp complete footprint to current monitor work area','corner':'preserve circle and minimally clamp both axes','origin':'final visible center is the armed hit-test origin','cursorWarp':False,'guard':'only when boundary clamping places the pointer outside the center zone, keep selection empty until center entry; early release cancels','smallWorkArea':'scale the complete canvas uniformly; validate reduced text and target sizes','invariants':['item order','item angles','orbit radius','icon size'],'layoutContract':'rendering and hit testing share final center and geometry'},
        'handoff':{'svg':'editable vector UI scenes with embedded verified source artwork','pdf':'embedded fonts and vector UI; official PNG sources retained for review','png':'review snapshots only','architecture':'one process; input/state/layout/theme/renderer/platform services separated','validation':'docs/validation.md'}
    }
    (VI/'tokens.json').write_text(json.dumps(tokens,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'pdf':str(path),'pages':len(pages),'svgFiles':len(list(VECTORS.glob('*.svg')))}))


if __name__=='__main__':
    main()
