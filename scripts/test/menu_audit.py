import subprocess, time, os
os.environ["DISPLAY"]=":99"
def xwins():
    out=subprocess.run("xdotool search --onlyvisible --name ''",shell=True,capture_output=True,text=True).stdout.split()
    d={}
    for w in out:
        nm=subprocess.run(["xdotool","getwindowname",w],capture_output=True,text=True).stdout.strip()
        if nm: d[w]=nm
    return d
def q(name,args=None):
    r=rpc("tools/call",{"name":name,"arguments":args or {}})["result"]
    return r.get("isError",False), "".join(c.get("text","") for c in r["content"])
sess=[0]
def fresh():
    sess[0]+=1
    q("new_session",{"name":"MenuTest6_%d"%sess[0]}); time.sleep(0.8)
    q("import_audio",{"track":"Audio 1","path":"/tmp/claude-0/scratch/test.wav"})
    q("create_midi_clip",{"track":"Synth 1","length_bars":2,"notes":[{"pitch":"C4","start_beat":0,"length_beats":1},{"pitch":"E4","start_beat":1,"length_beats":1},{"pitch":"E4","start_beat":1,"length_beats":1}]})
    q("add_drum_track",{"name":"Drums","bars":2})
    q("menu_command",{"path":"Window > Close All Floating Windows"})
def select():
    q("set_selection",{"start_bar":1,"end_bar":2,"tracks":["Audio 1","Synth 1","Drums"],"all_clips_in_range":True,"link_loop":False})
    e,t=q("get_selection")
    try: ok=t.count('"type"')>=3 or len(json.loads(t).get("clips",[]))>=3
    except Exception: ok="MIDI" in t and "test" in t
    if not ok:
        fresh(); q("set_selection",{"start_bar":1,"end_bar":2,"tracks":["Audio 1","Synth 1","Drums"],"all_clips_in_range":True,"link_loop":False})
fresh()
items=json.load(open("menu_items.json"))
SKIP=("File > New Session","File > Open Session","File > Open Recent","File > Revert","File > Show Session Folder","File > Import Session Data",
      "Track > Delete Selected","Claude > Connect","Claude > Open claude.ai","Help > Buy","Help > Open Setup Guide","Help > Open Sessions Folder",
      "Help > Enter Licence","Help > Licensed","Window > Close All")
base=xwins()
results=[]
seen={}
for it in items:
    p=it["path"]
    grp=" > ".join(p.split(" > ")[:2])
    if grp in ("Track > New Instrument Track with...","AudioSuite > Render through plugin..."):
        seen[grp]=seen.get(grp,0)+1
        if seen[grp]>4: continue
    if any(p.startswith(s) for s in SKIP): continue
    select()
    before=xwins()
    err,txt=q("menu_command",{"path":p})
    time.sleep(0.7)
    after=xwins()
    new={w:nm for w,nm in after.items() if w not in before}
    alive=subprocess.run("pgrep -x Squelchmoth",shell=True,capture_output=True).returncode==0
    if not alive: print("CRASH after",p); results.append((p,"CRASH",{})); break
    shot=""
    if new:
        shot="menushots6/%03d.png"%len(results)
        subprocess.run(["import","-window","root",shot])
    results.append((p,("ERR "+txt) if err else "ok",new,shot))
    print(("ERR " if err else "ok  ")+p+(" -> "+", ".join(new.values()) if new else "")+("  ["+txt[:120]+"]" if err else ""), flush=True)
    # dismiss dialogs (anything that is not a known floating window) with Escape
    for w,nm in new.items():
        subprocess.run(f"xdotool windowfocus --sync {w} key --clearmodifiers Escape",shell=True,capture_output=True)
    time.sleep(0.3)
    left={w:nm for w,nm in xwins().items() if w not in before}
    if left:
        q("menu_command",{"path":"Window > Close All Floating Windows"})
        time.sleep(0.3)
        still={w:nm for w,nm in xwins().items() if w not in before}
        if still: print("     still open:",list(still.values()))
json.dump(results,open("menu_results.json","w"),indent=1)
print("DONE",len(results))
