"""Generate the English VI PDF, SVG masters, and renderer tokens.

Run with Python + reportlab on Windows. PNG review snapshots are rendered from
the PDF with Poppler and are never used as the application's UI substrate.
"""
from scene import *
from osd_pages import build_pages, osd

def main():
    VECTORS.mkdir(parents=True,exist_ok=True)
    PDF_DIR.mkdir(parents=True,exist_ok=True)
    pages=build_pages()
    path=PDF_DIR/'copilot-reel-vi-v0.5.pdf'
    document=canvas.Canvas(str(path),pagesize=(PAGE_W*PDF_SCALE,PAGE_H*PDF_SCALE),pageCompression=1)
    document.setTitle('Copilot Reel - Unified Orbit and Native OSD v0.5')
    document.setAuthor('Copilot Reel')
    for page in pages:
        page.draw_pdf(document);document.showPage()
    document.save()
    for code,name,_ in SCHEMES:
        for theme in PALETTES:
            scene=Scene(420,420);wheel(scene,code,theme,1,210,210)
            (VECTORS/f'{code}-{theme}.svg').write_text(scene.svg(name+' / '+theme),encoding='utf-8',newline='\n')
        scene=Scene(128,128);logo(scene,code,0,0,128)
        (VECTORS/f'{code}-brand.svg').write_text(scene.svg(name+' / brand direction'),encoding='utf-8',newline='\n')
    for theme in ['light','dark']:
        for material in ['solid','frosted']:
            scene=Scene(420,420);wheel(scene,'orbit',theme,1,210,210,material=material)
            (VECTORS/f'orbit-{theme}-{material}.svg').write_text(scene.svg('Soft Orbit / '+theme+' / '+material+' tonal concept'),encoding='utf-8',newline='\n')
        for screen in ['home','settings','apps','editor','appearance']:
            scene=Scene(600,432);osd(scene,20,52,theme,screen)
            (VECTORS/f'osd-{screen}-{theme}.svg').write_text(scene.svg('OSD / '+screen+' / '+theme),encoding='utf-8',newline='\n')
    tokens={
        'schemaVersion':'0.5','status':'implemented-in-application-0.2.0-validation-tracked-separately',
        'units':'DIP','defaultScheme':'orbit','defaultLanguage':'en-US','supportedLanguages':['en-US','zh-CN'],
        'rendering':'native vector geometry and DirectWrite text',
        'typography':{'preferred':'Segoe UI Variable','fallback':'Segoe UI','cjk':'system fallback','applicationLabel':14,'statusLabel':12},
        'interaction':{'systemHoldThresholdMs':300,'centerCancelDiameter':88,'defaultItems':6,'releaseCommitsAtMostOnce':True,'gapsAreNotDeadZones':True},
        'motion':{'appearanceMs':100,'selectionMs':0,'exitMs':0,'osdPageMs':0,'appearanceScale':[.98,1.0],'reduceMotion':'instant state changes','futureCeilings':{'selectionMs':80,'osdPageMs':120},'note':'Only the appearance animation is implemented; selection and navigation are immediate.'},
        'colorPolicy':{'surface':'follow Windows app light/dark neutral theme','accent':'follow Windows accent for selected items','sampleLight':'#005FB8','sampleDark':'#60CDFF','highContrast':'use actual system colors; black/yellow previews are illustrative'},
        'systemAppearance':{'followSystem':True,'materialPriority':['high-contrast-system-solid','transparency-disabled-or-unavailable-theme-solid','transparency-enabled-and-supported-frosted'],'solid':{'surfaceAlpha':1.0,'backgroundSampling':False,'backgroundBlur':False},'frosted':{'surface':'single compositor backdrop layer','preview':'tonal illustration, not a live blur','availability':'validate nonactivating HWND and clipping'},'signals':['UISettings.AdvancedEffectsEnabledChanged','UISettings.ColorValuesChanged','UISettings.AnimationsEnabledChanged','WM_SETTINGCHANGE','WM_SYSCOLORCHANGE','WM_THEMECHANGED'],'delivery':'coalesce on UI thread; preserve the active gesture'},
        'palettes':PALETTES,
        'schemes':{'halo':{'name':'Fluent Halo','status':'comparison','diameter':320},'petals':{'name':'Command Petals','status':'comparison','designBounds':360},'orbit':{'name':'Soft Orbit','status':'selected','diameter':336,'icon':40,'orbitRadius':112,'selectionSeat':[60,64],'selectedScaleMax':1.03}},
        'iconography':{'status':'implemented','renderer':'shared DrawAppSymbol Direct2D geometry','tileSize':40,'tileRadius':10,'symbolSize':28,'strokeWidth':1.75,'highContrastStrokeWidth':2,'selectedColor':'Windows accent; system highlighted text in high contrast','identity':'visible app name first, then launch target','unknown':'neutral application-window symbol','shellBitmaps':False,'initialsFallback':False,'osdListSymbolSize':22},
        'osd':{'status':'implemented','logicalClientSize':[560,360],'frame':'native Windows caption and frame','contentInset':24,'rowHeight':47,'nativeControls':['button','listbox','edit','combobox','overflow-scrollbars'],'primary':['Apps','Appearance','Settings'],'languageLocation':'Settings / Language','settingsItems':['Language','About Copilot Reel'],'applicationVersion':'0.2.0','autostartSetting':False,'editorAdvancedSheet':['launchMethod','arguments','workingDirectory'],'placement':'center within the current monitor work area','smallWorkArea':'scrollable logical client area; do not shrink text'},
        'edgeLayout':{'status':'implemented','interior':'full pointer-centered orbit','edge':'minimal whole-circle translation inside monitor work area','corner':'minimal whole-circle translation on both axes','origin':'relocated visual center is the only armed hit-test origin','cursorWarp':False,'guard':'if the original pointer is outside the relocated center zone, remain unselected until it enters; early release cancels','timerRecenter':False,'smallWorkArea':'open the scrollable OSD when the complete wheel cannot fit','invariants':['item order','item angles','orbit radius','icon size'],'layoutContract':'rendering and hit testing share the same center and geometry','tradeoff':'edge activation may require an inward movement before direction selection'},
        'handoff':{'appSymbols':'shared monochrome native vector symbols; unknown targets use a neutral app-window mark','svgText':'editable text with system-font references','pdf':'embedded fonts and vector shapes','png':'review snapshots only','osdScenes':'layout illustration; native caption and input-control details vary with Windows','backdrop':'independent native compositor effect','architecture':'single process; input/state/shared-layout/theme-policy/renderer/services separation'}
    }
    (VI/'tokens.json').write_text(json.dumps(tokens,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'pdf':str(path),'pages':len(pages),'svgFiles':len(list(VECTORS.glob('*.svg')))}))

if __name__=='__main__':main()
