// mcp_call.js - calls one tool of the Unreal Editor's MCP server over HTTP and prints the result.
// For chats whose own MCP connection to the Editor is not up (the Editor was opened after the chat started).
//
//   node Tools/Jam/mcp_call.js <tool> [argsFile.json] [--port 8059] [--raw]
//   node Tools/Jam/mcp_call.js py <script.py>          runs the Python file in the Editor (execute_python)
//
//   <tool>      top-level MCP tool: list_toolsets, describe_toolsets, describe_tools, call_tool
//   argsFile    JSON file with the tool's arguments (default: {}); for call_tool:
//               {"toolset_name": "...", "tool_name": "...", "arguments": {...}}
//   --raw       print the whole JSON-RPC result instead of the text parts.
// A Python script returns the value of its last expression; it writes its own files (renders, lists).
const fs = require('fs');
const path = require('path');

const argv = process.argv.slice(2);
function opt(name, def) {
  const i = argv.indexOf(name);
  if (i < 0) return def;
  const v = argv[i + 1];
  argv.splice(i, 2);
  return v;
}
function flag(name) {
  const i = argv.indexOf(name);
  if (i < 0) return false;
  argv.splice(i, 1);
  return true;
}
const port = opt('--port', '8059');
const outDir = opt('--out', null);
const prefix = opt('--prefix', 'img');
const pyTool = opt('--py', null);
const codeArg = opt('--code-arg', 'code');
const raw = flag('--raw');
const tool = argv[0];
const argsFile = argv[1];
if (!tool) {
  console.error('usage: node mcp_call.js <tool> [argsFile] [--py <pythonToolName>] [--out dir] [--prefix p] [--port n] [--raw]');
  process.exit(2);
}
const url = 'http://127.0.0.1:' + port + '/mcp';

async function post(body, sid) {
  const headers = { 'Content-Type': 'application/json', Accept: 'application/json, text/event-stream' };
  if (sid) headers['Mcp-Session-Id'] = sid;
  const r = await fetch(url, { method: 'POST', headers, body: JSON.stringify(body) });
  let txt = await r.text();
  if (/^\s*(event:|data:)/.test(txt)) {
    txt = txt.split('\n').filter((l) => l.startsWith('data:')).map((l) => l.slice(5)).join('');
  }
  return { json: txt.trim() ? JSON.parse(txt) : null, sid: r.headers.get('mcp-session-id') };
}

(async () => {
  const init = await post({ jsonrpc: '2.0', id: 1, method: 'initialize', params: { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'jam-mcp-call', version: '1' } } }, null);
  const sid = init.sid;
  await post({ jsonrpc: '2.0', method: 'notifications/initialized' }, sid);

  let args = {};
  if (argsFile) {
    const text = fs.readFileSync(argsFile, 'utf8').replace(/^﻿/, '');
    if (tool === 'py') {
      args = { toolset_name: 'editor_toolset.toolsets.python_scripting.PythonScriptingTools', tool_name: 'execute_python', arguments: { script: { content: text, language: 'Python' } } };
    } else {
      args = JSON.parse(text);
    }
  }
  const limit = Date.now() + 5 * 60 * 1000;
  let res;
  for (;;) {
    res = await post({ jsonrpc: '2.0', id: 2, method: 'tools/call', params: { name: tool === 'py' ? 'call_tool' : tool, arguments: args } }, sid);
    const first = res.json && res.json.result && res.json.result.content && res.json.result.content[0];
    const t = first && first.text ? String(first.text) : '';
    if (!/slow task/i.test(t) || Date.now() > limit) break;
    await new Promise((ok) => setTimeout(ok, 5000));
  }
  if (raw) {
    console.log(JSON.stringify(res.json, null, 1).slice(0, 200000));
    return;
  }
  if (!res.json) { console.log('(empty response)'); return; }
  if (res.json.error) { console.log('ERROR ' + JSON.stringify(res.json.error)); process.exitCode = 1; return; }
  const content = (res.json.result && res.json.result.content) || [];
  let n = 0;
  for (const c of content) {
    if (c.type === 'text') {
      // A Python script's result comes as {"returnValue": "..."}: print the string itself, with its line breaks.
      let text = c.text;
      try {
        const parsed = JSON.parse(text);
        if (parsed && typeof parsed.returnValue === 'string' && Object.keys(parsed).length === 1) text = parsed.returnValue;
      } catch (e) { /* not JSON: print as is */ }
      console.log(text);
    }
    else if (c.type === 'image' && outDir) {
      fs.mkdirSync(outDir, { recursive: true });
      const p = path.join(outDir, prefix + '_' + n++ + '.png');
      fs.writeFileSync(p, Buffer.from(c.data, 'base64'));
      console.log('[image] ' + p);
    } else console.log('[' + c.type + ']');
  }
  if (res.json.result && res.json.result.isError) process.exitCode = 1;
})().catch((e) => { console.error('FAILED ' + e.message); process.exit(1); });
