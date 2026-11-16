import json,sys,urllib.request,re,base64
tok=re.search(r'mcpToken" val="([^"]+)',open('/root/Squelchmoth/Squelchmoth.settings').read()).group(1)
URL=f"http://127.0.0.1:7383/mcp/{tok}"
sid=None; n=0
def rpc(method,params=None):
    global sid,n
    n+=1
    body=json.dumps({"jsonrpc":"2.0","id":n,"method":method,"params":params or {}}).encode()
    h={"Content-Type":"application/json","Accept":"application/json, text/event-stream","MCP-Protocol-Version":"2025-06-18"}
    if sid: h["Mcp-Session-Id"]=sid
    r=urllib.request.urlopen(urllib.request.Request(URL,body,h),timeout=300)
    sid=r.headers.get("Mcp-Session-Id") or sid
    return json.loads(r.read())
def call(name,args=None,out=None):
    r=rpc("tools/call",{"name":name,"arguments":args or {}})["result"]
    for c in r["content"]:
        if c["type"]=="text": print(("ERR " if r.get("isError") else "")+name+": "+c["text"][:400].replace("\n"," "))
        if c["type"]=="image" and out:
            open(out,"wb").write(base64.b64decode(c["data"])); print("saved",out)
    return r
if __name__=="__main__":
    print(rpc("initialize",{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"t","version":"1"}})["result"]["serverInfo"])
    exec(open(sys.argv[1]).read())
