import {readFile,writeFile} from 'node:fs/promises';
import {homedir} from 'node:os';
import {join} from 'node:path';
import puppeteer from 'puppeteer-core';
const [port,path]=(await readFile(join(homedir(),'Library/Application Support/Google/Chrome/DevToolsActivePort'),'utf8')).trim().split('\n');
const browser=await puppeteer.connect({browserWSEndpoint:`ws://127.0.0.1:${port}${path}`,defaultViewport:null});
try{
 const page=(await browser.pages()).find(p=>p.url().startsWith('https://cml.kappino.me/'));
 const result=await page.evaluate(async()=>{
  if(location.origin!=='https://cml.kappino.me')throw Error('Origine errata');
  const token=JSON.parse(localStorage.auth).authtoken;
  const lab='1fc0e59b-0dfe-4908-8cae-c7bc470a0471';
  const api=async(path,method='GET',body)=>{const r=await fetch('/api/v0'+path,{method,headers:{Authorization:'Bearer '+token,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(20000)});const t=await r.text();if(!r.ok)throw Error(`CML ${r.status}: ${t}`);return t?JSON.parse(t):null;};
  const nodes=await api('/nodes');
  let n=nodes.find(n=>n.lab_id===lab&&n.label==='SW-PIANO2');
  if(!n)n=await api(`/labs/${lab}/nodes`,'POST',{label:'SW-PIANO2',node_definition:'iosvl2',image_definition:'iosvl2-2020',x:400,y:0,configuration:'hostname SW-PIANO2\nno ip domain lookup\nno ip routing\nline con 0\n logging synchronous\n no login\nend\n',tags:['tnes','console-test']});
  const count=(await api('/nodes')).filter(x=>!['STOPPED','DEFINED_ON_CORE'].includes(x.state)&&x.id!==n.id).length+1;
  if(count>5)throw Error('Limite 5 nodi');
  if(!['STARTED','BOOTED'].includes(n.state))await api(`/labs/${lab}/nodes/${n.id}/state/start`,'PUT');
  return {lab_id:lab,node_id:n.id,label:n.label,maximum_nodes:count};
 });
 await writeFile(new URL('../iosvl2-test-node.json',import.meta.url),JSON.stringify(result,null,2));console.log(result);
 for(let i=0;i<24;i++){
  const state=await page.evaluate(async({lab_id,node_id})=>{
   if(location.origin!=='https://cml.kappino.me')throw Error('Origine errata');
   const base='/api/v0/labs/'+lab_id+'/nodes/'+node_id;
   const headers={Authorization:'Bearer '+JSON.parse(localStorage.auth).authtoken};
   const n=await (await fetch(base,{headers})).json();
   const r=await fetch(base+'/consoles/0/log?lines=30',{headers});
   return {state:n.state,log:r.ok?await r.json():''};
  },result);
  console.log(JSON.stringify(state));
  await writeFile(new URL('../iosvl2-boot.json',import.meta.url),JSON.stringify(state,null,2));
  if(/SW-PIANO2[>#]/.test(state.log))break;
  await new Promise(r=>setTimeout(r,10000));
 }
 const response=await fetch('https://api-cml.kappino.me/api/terminal/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({node:'SW-PIANO2',command:'show ip interface brief'}),signal:AbortSignal.timeout(35000)});
 const proof=await response.json();await writeFile(new URL('../iosvl2-command-proof.json',import.meta.url),JSON.stringify(proof,null,2));console.log(JSON.stringify(proof));
}finally{await browser.disconnect();}
